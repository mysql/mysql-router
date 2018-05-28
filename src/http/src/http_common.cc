/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * API Facade around libevent's http interface
 */

#include <iostream>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include "mysqlrouter/http_common.h"
#include "http_request_impl.h"


// wrap evhttp_uri

struct HttpUri::impl {
  std::unique_ptr<evhttp_uri, std::function<void(evhttp_uri *)>> uri;
};

HttpUri::HttpUri(std::unique_ptr<evhttp_uri, std::function<void(evhttp_uri *)>> uri) {
  pImpl.reset(new impl { std::move(uri) });
}

HttpUri::HttpUri(HttpUri &&) = default;
HttpUri::~HttpUri() = default;

HttpUri HttpUri::parse(const std::string &uri_str) {
  // wrap a owned pointer
  return HttpUri {
    std::unique_ptr<evhttp_uri, decltype(&evhttp_uri_free)> {
      evhttp_uri_parse(uri_str.c_str()), &evhttp_uri_free }
  };
}

std::string HttpUri::get_path() const {
  return evhttp_uri_get_path(pImpl->uri.get());
}


// wrap evbuffer

struct HttpBuffer::impl {
  std::unique_ptr<evbuffer, std::function<void(evbuffer *)>> buffer;
};

// non-owning pointer
HttpBuffer::HttpBuffer(std::unique_ptr<evbuffer, std::function<void(evbuffer *)>> buffer) {
  pImpl.reset(new impl { std::move(buffer) });
}

void HttpBuffer::add(const char *data, size_t data_size) {
  evbuffer_add(pImpl->buffer.get(), data, data_size);
}

void HttpBuffer::add_file(int file_fd, off_t offset, off_t size) {
  evbuffer_add_file(pImpl->buffer.get(), file_fd, offset, size);
}

size_t HttpBuffer::length() const {
  return evbuffer_get_length(pImpl->buffer.get());
}

std::vector<uint8_t> HttpBuffer::pop_front(size_t length) {
  std::vector<uint8_t> data;
  data.resize(length);

  int bytes_read;
  if (-1 == (bytes_read = evbuffer_remove(pImpl->buffer.get(), data.data(), data.size()))) {
    throw std::runtime_error("...");
  }

  data.resize(bytes_read);
  data.shrink_to_fit();

  return data;
}


HttpBuffer::HttpBuffer(HttpBuffer &&)  = default;
HttpBuffer::~HttpBuffer() = default;


// wrap evkeyvalq

struct HttpHeaders::impl {
  std::unique_ptr<evkeyvalq, std::function<void(evkeyvalq *)>> hdrs;
};

HttpHeaders::HttpHeaders(std::unique_ptr<evkeyvalq, std::function<void(evkeyvalq *)>> hdrs) {
  pImpl.reset(new impl { std::move(hdrs) });
}

int HttpHeaders::add(const char *key, const char *value) {
  return evhttp_add_header(pImpl->hdrs.get(), key, value);
}

const char *HttpHeaders::get(const char *key) const {
  return evhttp_find_header(pImpl->hdrs.get(), key);
}

std::pair<std::string, std::string> HttpHeaders::Iterator::operator *() {
  return { node_->key, node_->value };
}

HttpHeaders::Iterator& HttpHeaders::Iterator::operator ++() {
  node_ = node_->next.tqe_next;

  return *this;
}

bool HttpHeaders::Iterator::operator!=(const Iterator &it) const {
  return node_ != it.node_;
}

HttpHeaders::Iterator HttpHeaders::begin() {
  return pImpl->hdrs->tqh_first;
}

HttpHeaders::Iterator HttpHeaders::end() {
  return *(pImpl->hdrs->tqh_last);
}

HttpHeaders::HttpHeaders(HttpHeaders &&) = default;
HttpHeaders::~HttpHeaders() = default;


// wrap evhttp_request

HttpRequest::HttpRequest(std::unique_ptr<evhttp_request, std::function<void(evhttp_request *)>> req)
{
  pImpl.reset(new impl { std::move(req) });
}

struct RequestHandlerCtx {
  HttpRequest *req;
  HttpRequest::RequestHandler cb;
  void *cb_data;
};

HttpRequest::RequestHandler HttpRequest::sync_callback = [](HttpRequest *req, void *){
  // if connection was successful, keep the request-object alive past this
  // request-handler lifetime
  evhttp_request *ev_req = req->pImpl->req.get();
  if (ev_req) {
    evhttp_request_own(ev_req);
  }
};

HttpRequest::HttpRequest(HttpRequest::RequestHandler cb, void *cb_arg) {
  auto *ev_req = evhttp_request_new(
      [](evhttp_request *req, void *ev_cb_arg) {
        auto *ctx = static_cast<RequestHandlerCtx *>(ev_cb_arg);

        ctx->req->pImpl->req.release(); // the old request object may already be free()ed in case of error
        ctx->req->pImpl->req.reset(req); // just update with what we have
        ctx->cb(ctx->req, ctx->cb_data);

        delete ctx;
      }, new RequestHandlerCtx{this, cb, cb_arg});

#if LIBEVENT_VERSION_NUMBER > 0x02010000
  evhttp_request_set_error_cb(ev_req, [](evhttp_request_error err_code, void *ev_cb_arg){
      auto *ctx = static_cast<RequestHandlerCtx *>(ev_cb_arg);

      ctx->req->error_code(err_code);
      });
#endif

  pImpl.reset(new impl {
      std::unique_ptr<evhttp_request, std::function<void(evhttp_request *)>>(
        ev_req, evhttp_request_free)});
}

HttpRequest::HttpRequest(HttpRequest &&) = default;
HttpRequest::~HttpRequest() = default;

void HttpRequest::send_error(int status_code, std::string status_text) {
  evhttp_send_error(pImpl->req.get(), status_code, status_text.c_str());
}

void HttpRequest::send_reply(int status_code, std::string status_text, HttpBuffer &chunk) {
  evhttp_send_reply(pImpl->req.get(), status_code, status_text.c_str(), chunk.pImpl->buffer.get());
}

void HttpRequest::send_reply(int status_code, std::string status_text) {
  evhttp_send_reply(pImpl->req.get(), status_code, status_text.c_str(), nullptr);
}

HttpRequest::operator bool() {
  return pImpl->req.operator bool();
}

void HttpRequest::error_code(int err_code) {
  pImpl->error_code = err_code;
}

int HttpRequest::error_code() {
  return pImpl->error_code;
}

std::string HttpRequest::error_msg() {
  switch(pImpl->error_code) {
  case EVREQ_HTTP_TIMEOUT: return "timeout";
  case EVREQ_HTTP_EOF: return "eof";
  case EVREQ_HTTP_INVALID_HEADER: return "invalid-header";
  case EVREQ_HTTP_BUFFER_ERROR: return "buffer-error";
  case EVREQ_HTTP_REQUEST_CANCEL: return "request-cancel";
  case EVREQ_HTTP_DATA_TOO_LONG: return "data-too-long";
  default: return "unknown";
  }
}


std::string HttpRequest::get_uri() const {
  return evhttp_request_get_uri(pImpl->req.get());
}

HttpHeaders HttpRequest::get_output_headers() {
  auto *ev_req = pImpl->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }
  // wrap a non-owned pointer
  return std::unique_ptr<evkeyvalq, std::function<void(evkeyvalq *)>>(
      evhttp_request_get_output_headers(ev_req),
         [](evkeyvalq *){});
}

HttpHeaders HttpRequest::get_input_headers() const {
  auto *ev_req = pImpl->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }
  // wrap a non-owned pointer
  return std::unique_ptr<evkeyvalq, std::function<void(evkeyvalq *)>>(
      evhttp_request_get_input_headers(ev_req),
         [](evkeyvalq *){});
}

HttpBuffer HttpRequest::get_output_buffer() {
  // wrap a non-owned pointer
  auto *ev_req = pImpl->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

  return std::unique_ptr<evbuffer, std::function<void(evbuffer *)>>(
      evhttp_request_get_output_buffer(ev_req), [](evbuffer *){});
}

unsigned HttpRequest::get_response_code() const {
  // wrap a non-owned pointer
  auto *ev_req = pImpl->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

  return evhttp_request_get_response_code(ev_req);
}

std::string HttpRequest::get_response_code_line() const {
  // wrap a non-owned pointer
  auto *ev_req = pImpl->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

  return evhttp_request_get_response_code_line(ev_req);
}


HttpBuffer HttpRequest::get_input_buffer() const {
  auto *ev_req = pImpl->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

  // wrap a non-owned pointer
  return std::unique_ptr<evbuffer, std::function<void(evbuffer *)>>(
      evhttp_request_get_input_buffer(ev_req),
         [](evbuffer *){});
}

HttpMethod::type HttpRequest::get_method() const {
  return evhttp_request_get_command(pImpl->req.get());
}
