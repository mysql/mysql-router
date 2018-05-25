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

#ifndef MYSQLROUTER_HTTP_COMMON_INCLUDED
#define MYSQLROUTER_HTTP_COMMON_INCLUDED

#include <ctime>
#include <memory>
#include <functional>  // std::function
#include <vector>
#include <bitset>

struct evhttp_uri;
struct evhttp_request;
struct evkeyvalq;
struct evkeyval;
struct evbuffer;

class HttpServer;

// http_common.cc


// https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
namespace HttpStatusCode {
  constexpr int Continue = 100;  // RFC 7231
  constexpr int SwitchingProtocols = 101;  // RFC 7231
  constexpr int Processing = 102;  // RFC 2518
  constexpr int EarlyHints = 103;  // RFC 8297

  constexpr int Ok = 200;  // RFC 7231
  constexpr int Created = 201;  // RFC 7231
  constexpr int Accepted = 202;  // RFC 7231
  constexpr int NonAuthoritiveInformation = 203;  // RFC 7231
  constexpr int NoContent = 204;  // RFC 7231
  constexpr int ResetContent = 205;  // RFC 7231
  constexpr int PartialContent = 206;  // RFC 7233
  constexpr int MultiStatus = 207;  // RFC 4918
  constexpr int AlreadyReported = 208;  // RFC 5842
  constexpr int InstanceManipulationUsed = 226;  // RFC 3229

  constexpr int MultipleChoices = 300;  // RFC 7231 
  constexpr int MovedPermanently = 301;  // RFC 7231 
  constexpr int Found = 302;  // RFC 7231 
  constexpr int SeeOther = 303;  // RFC 7231 
  constexpr int NotModified = 304;  // RFC 7232 
  constexpr int UseProxy = 305;  // RFC 7231
  constexpr int TemporaryRedirect = 307;  // RFC 7231 
  constexpr int PermanentRedirect = 308;  // RFC 7538 

  constexpr int BadRequest = 400;  // RFC 7231 
  constexpr int Unauthorized = 401;  // RFC 7235 
  constexpr int PaymentRequired = 402;  // RFC 7231 
  constexpr int Forbidden = 403;  // RFC 7231 
  constexpr int NotFound = 404;  // RFC 7231
  constexpr int MethodNotAllowed = 405;  // RFC 7231
  constexpr int NotAcceptable = 406;  // RFC 7231
  constexpr int ProxyAuthenticationRequired = 407;  // RFC 7235
  constexpr int RequestTimeout = 408;  // RFC 7231
  constexpr int Conflicts = 409;  // RFC 7231
  constexpr int Gone = 410;  // RFC 7231
  constexpr int LengthRequired = 411;  // RFC 7231
  constexpr int PreconditionFailed = 412;  // RFC 7232
  constexpr int PayloadTooLarge = 413;  // RFC 7231
  constexpr int URITooLarge = 414;  // RFC 7231
  constexpr int UnsupportedMediaType = 415;  // RFC 7231
  constexpr int RangeNotSatisfiable = 416;  // RFC 7233
  constexpr int ExpectationFailed = 417;  // RFC 7231
  constexpr int IamaTeapot = 418;  // RFC 7168
  constexpr int MisdirectedRequest = 421;  // RFC 7540
  constexpr int UnprocessableEntity = 422;  // RFC 4918
  constexpr int Locked = 423;  // RFC 4918
  constexpr int FailedDependency = 424;  // RFC 4918
  constexpr int UpgradeRequired = 426;  // RFC 7231
  constexpr int PreconditionRequired = 428;  // RFC 6585
  constexpr int TooManyRequests = 429;  // RFC 6585
  constexpr int RequestHeaderFieldsTooLarge = 431;  // RFC 6585
  constexpr int UnavailableForLegalReasons = 451;  // RFC 7725

  constexpr int InternalError = 500;  // RFC 7231
  constexpr int NotImplemented = 501;  // RFC 7231
  constexpr int BadGateway = 502;  // RFC 7231
  constexpr int ServiceUnavailable = 503;  // RFC 7231
  constexpr int GatewayTimeout = 504;  // RFC 7231
  constexpr int HTTPVersionNotSupported = 505;  // RFC 7231
  constexpr int VariantAlsoNegotiates = 506;  // RFC 2295
  constexpr int InsufficientStorage = 507;  // RFC 4918
  constexpr int LoopDetected = 508;  // RFC 5842
  constexpr int NotExtended = 510;  // RFC 2774
  constexpr int NetworkAuthorizationRequired = 511;  // RFC 6585
};

class HttpUri {
  struct impl;

  std::unique_ptr<impl> pImpl;

public:
  HttpUri(std::unique_ptr<evhttp_uri, std::function<void(evhttp_uri *)>> uri);
  HttpUri(HttpUri &&);
  ~HttpUri();
  static HttpUri parse(const std::string &uri_str);

  std::string get_path() const;
};

// wrapper around evbuffer
class HttpBuffer {
  struct impl;

  std::unique_ptr<impl> pImpl;

  friend class HttpRequest;
public:
  HttpBuffer(std::unique_ptr<evbuffer, std::function<void(evbuffer *)>> buffer);

  HttpBuffer(HttpBuffer &&);

  ~HttpBuffer();

  void add(const char *data, size_t data_size);
  void add_file(int file_fd, off_t offset, off_t size);

  size_t length() const;
  std::vector<uint8_t> pop_front(size_t length);
};

class HttpHeaders {
  struct impl;

  std::unique_ptr<impl> pImpl;
public:
  class Iterator {
    evkeyval *node_;
  public:
    Iterator(evkeyval *node):
      node_{node}
    {};
    std::pair<std::string, std::string> operator*();
    Iterator& operator++();
    bool operator!=(const Iterator &it) const;
  };
  HttpHeaders(std::unique_ptr<evkeyvalq, std::function<void(evkeyvalq *)>> hdrs);
  HttpHeaders(HttpHeaders &&);

  ~HttpHeaders();
  int add(const char *key, const char *value);
  const char *get(const char *key) const;

  Iterator begin();
  Iterator end();
};

namespace HttpMethod {
  using type = int;
  using pos_type = unsigned;
  namespace Pos {
    constexpr pos_type GET = 0;
    constexpr pos_type POST = 1;
    constexpr pos_type HEAD = 2;
    constexpr pos_type PUT = 3;
    constexpr pos_type DELETE = 4;
    constexpr pos_type OPTIONS = 5;
    constexpr pos_type TRACE = 6;
    constexpr pos_type CONNECT = 7;
    constexpr pos_type PATCH = 8;

    constexpr unsigned _LAST = PATCH;
  };
  using Bitset = std::bitset<Pos::_LAST>;

  constexpr type GET { 1 << Pos::GET };
  constexpr type POST { 1 << Pos::POST };
  constexpr type HEAD { 1 << Pos::HEAD };
  constexpr type PUT { 1 << Pos::PUT };
  constexpr type DELETE { 1 << Pos::DELETE };
  constexpr type OPTIONS { 1 << Pos::OPTIONS };
  constexpr type TRACE { 1 << Pos::TRACE };
  constexpr type CONNECT { 1 << Pos::CONNECT };
  constexpr type PATCH { 1 << Pos::PATCH };
};

class IOContext {
  class impl;

  std::unique_ptr<impl> pImpl;
  friend class HttpClient;
public:
  IOContext();
  ~IOContext();
  void dispatch();
};

class HttpRequest {
  class impl;

  std::unique_ptr<impl> pImpl;

  friend class HttpClient;
public:
  using RequestHandler = void (*)(HttpRequest *, void *);

  HttpRequest(RequestHandler cb, void *arg = nullptr);
  HttpRequest(std::unique_ptr<evhttp_request, std::function<void(evhttp_request *)>> req);
  HttpRequest(HttpRequest &&);
  ~HttpRequest();

  HttpHeaders get_output_headers();
  HttpHeaders get_input_headers() const;
  HttpBuffer get_output_buffer();
  HttpBuffer get_input_buffer() const;

  unsigned get_response_code() const;
  std::string get_response_code_line() const;

  HttpMethod::type get_method() const;

  std::string get_uri() const;

  void send_reply(int status_code, std::string status_text);
  void send_reply(int status_code, std::string status_text, HttpBuffer &buffer);

  void send_error(int status_code, std::string status_text);

  static RequestHandler sync_callback;

  operator bool();

  int error_code();
  void error_code(int);
  std::string error_msg();
};

class HttpClient {
  class impl;

  std::unique_ptr<impl> pImpl;

  IOContext &io_ctx_;
public:
  HttpClient();
  ~HttpClient();
  HttpClient(IOContext &io_ctx, const std::string &address, uint16_t port);

  void make_request(HttpRequest *req, HttpMethod::type method, const std::string &uri);
  void make_request_sync(HttpRequest *req, HttpMethod::type method, const std::string &uri);
};

// http_time.cc

bool is_modified_since(const HttpRequest &req, time_t last_modified);
void add_last_modified(HttpRequest &req, time_t last_modified);

#endif
