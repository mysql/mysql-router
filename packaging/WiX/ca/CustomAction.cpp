/* Copyright (c) 2010, 2017 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <winreg.h>
#include <msi.h>
#include <msiquery.h>
#include <wcautil.h>
#include <string.h>
#include <strsafe.h>
#include <direct.h>
#include <stdlib.h>
#include <tchar.h>
#include "nt_servc.h"


int stop_service(const char *service_name)
{
  int stopped = 0;
  WcaLog(LOGMSG_STANDARD, "Trying to stop the service.");
  SC_HANDLE hSCM = NULL;
  hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  if (hSCM != NULL) {
    SC_HANDLE hService = NULL;
    hService = OpenServiceA(hSCM, service_name, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
    if (hService != NULL) {
      WcaLog(LOGMSG_STANDARD, "Waiting for the service to stop...");
      SERVICE_STATUS status;
      /* Attempt to stop the service */
      if (ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        /* Now wait until it's stopped */
        while ("it's one big, mean and cruel world out there") {
          if (!QueryServiceStatus(hService, &status)) {
            WcaLog(LOGMSG_STANDARD, "Error while querying service status in 'stop_service' (code %d)", GetLastError());
            break;
          }
          if (status.dwCurrentState == SERVICE_STOPPED) break;
          Sleep(1000);
        }
        WcaLog(LOGMSG_STANDARD, "Stopped the service.");
        stopped = 1;
      }
      /* Mark the service for deletion */
      DeleteService(hService);
      CloseServiceHandle(hService);
    }
    CloseServiceHandle(hSCM);
  }  
  return stopped;
}

int remove_service(const TCHAR *service_name) {

  int result = 0;
  SC_HANDLE service;
  SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if(hSCM == NULL) { 
    WcaLog(LOGMSG_STANDARD, "Failed to open the Service Control Mananger (with code %d)\n", GetLastError());
    return result;
  }
  service = OpenService(hSCM, service_name, DELETE);
  if (service == NULL) {
    WcaLog(LOGMSG_STANDARD, "Failed to open the service to delete (with code %d)\n", GetLastError());
    CloseServiceHandle(hSCM);
    return 1;
  }
  if (!DeleteService(service)) {
    DWORD code = GetLastError();
    WcaLog(LOGMSG_STANDARD, "Failed to delete the service after opening it (with code %d)\n", code);
    if (code == ERROR_SERVICE_MARKED_FOR_DELETE) {
      MessageBox(NULL, L"MySQL Router is marked for deletion, but needs to restart computer to remove it.", L"Info", 0);
      result = 1;
    }
  } else {
    WcaLog(LOGMSG_STANDARD, "Service deleted successfully\n");
    result = 1;
  }
  
  CloseServiceHandle(service);
  CloseServiceHandle(hSCM);

  return result;
}


UINT wrap(MSIHANDLE hInstall, char *name, int check_only) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, name);
  ExitOnFailure(hr, "Failed to initialize");

  WcaLog(LOGMSG_STANDARD, "Initialized.");

  LPCTSTR szInternName = TEXT("MySQLRouter");
  int rc = remove_service(szInternName);
	if (!rc) {
	  er = ERROR_CANCELLED;
	}

LExit:
  WcaSetReturnValue(er);
  return WcaFinalize(er);
}

UINT __stdcall RemoveServiceNoninteractive(MSIHANDLE hInstall) {
  return wrap(hInstall, "RemoveServiceNoninteractive", -1);
}

UINT __stdcall RemoveService(MSIHANDLE hInstall) {
  return wrap(hInstall, "RemoveService", 0);
}

UINT __stdcall TestService(MSIHANDLE hInstall) {
  return wrap(hInstall, "TestService", 1);
}

// We implement a simple string buffer class to avoid having to link to MSVCR
class StringBuffer {
public:
	StringBuffer()	{
		size_ = 1024;
		length_ = 0;
		buffer_ = (char*)malloc(size_);
	}

	~StringBuffer() {
		if (buffer_)
			free(buffer_);
	}

	void append(const char *s) {
		append(s, strlen(s));
	}

	void append(const char *s, size_t length)	{
		if (length_ + length + 1 > size_)	{
			char *tmp = (char*)realloc(buffer_, size_ + length + 1);
			size_ += length + 1;
			if (!tmp)	{
				free(buffer_);
				buffer_ = NULL;
			}
      else {
        buffer_ = tmp;
      }
		}
		if (buffer_) {
			memcpy(buffer_ + length_, s, length);
			length_ += length;
			buffer_[length_] = 0;
		}
	}

	const char *c_str() const {
		return buffer_;
	}

	char *release()	{
		char *tmp = buffer_;
		buffer_ = NULL;
		return tmp;
	}

private:
	char *buffer_;
	size_t length_;
	size_t size_;
};


static char *replace_variable(char *data, const char *name, const char *value) {
  char *start = data;
	char *ptr = NULL;
  char *end = data + strlen(data);
	size_t name_len = strlen(name);
	size_t value_len = strlen(value);
	StringBuffer result;

  while ((ptr = strstr(start, name)))	{
		result.append(start, ptr-start);
		result.append(value, value_len);
		start = ptr + name_len;
  }
	result.append(start, strlen(start));

	return result.release();
}


static bool dir_exists(char *dir_path)
{
  DWORD dwAttrib = GetFileAttributesA(dir_path);

  if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
    if ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      WcaLog(LOGMSG_STANDARD, "Error: the path '%s' exists as a file (must be a directory)\n", dir_path);
      return false;
    } else {
      return true;
    }
  } else {
    return false;
  }
}


static DWORD create_directory_recursively(const char *path)
{
  DWORD code = 0;
  // ensure we have writeable copy for the string
  HLOCAL buf = LocalAlloc(0, strlen(path) + 1);
  if (buf == NULL) {
    return GetLastError();
  }
  strcpy(reinterpret_cast<char *>(buf), path);
  char *pc, *pc_base;
  pc = pc_base = reinterpret_cast<char *>(buf);
  while (*pc)
  {
    if (*pc == '\\' || *pc == '/')
    {
      *pc = '\0';
      if (!dir_exists(pc_base))
      {
        if (!CreateDirectoryA(pc_base, NULL))
        {
          code = GetLastError();
        }
      }
      *pc = '\\';
      if (code != 0) goto end;
    }
    ++pc;
  }

  if (!dir_exists(pc_base))
  {
    if (!CreateDirectoryA(pc_base, NULL))
    {
      code = GetLastError();
    }
  }
end:
  LocalFree(buf);
  return code;
}


/* Copies the default config file from the install dir's etc folder to the proper place in
  ProgramData */
int install_config_file(const char *install_dir, const char* progdata_dir) {
  DWORD code;
  if ((code = create_directory_recursively(progdata_dir)) != 0)
  {
    WcaLog(LOGMSG_STANDARD, "Creating some directory part of the path '%s' failed with error %d", progdata_dir, code);
    return -1;
  }

	StringBuffer source_path;
	source_path.append(install_dir);
	source_path.append("\\etc\\mysqlrouter.conf.sample");
  FILE *f = fopen(source_path.c_str(), "r");
  if (!f) {
    WcaLog(LOGMSG_STANDARD, "Can't open config file template %s: %s", source_path.c_str(), strerror(errno));
    return -1;
  }
  char template_data[64 * 1024];
  size_t len;
  len = fread(template_data, 1, sizeof(template_data), f);
  if (len < 0) {
    WcaLog(LOGMSG_STANDARD, "Could not read config file template data from %s: %i", source_path, errno);
    fclose(f);
    return -1;
  }
  fclose(f);
  template_data[len] = 0;

	char *buffer;
  // do find/replace of variables
  buffer = replace_variable(template_data, "%INSTALL_FOLDER%", install_dir);
	if (buffer) {
		char *buffer2;
  	buffer2 = replace_variable(buffer, "%PROGRAMDATA_FOLDER%", progdata_dir);
		free(buffer);
		buffer = buffer2;
	}
	if (!buffer) {
		WcaLog(LOGMSG_STANDARD, "Out of memory creating default config file");
		return -1;
	}
	// write the config file
	StringBuffer target_file;
	target_file.append(progdata_dir);
	target_file.append("\\mysqlrouter.conf");
	FILE *tmp;
	if ((tmp = fopen(target_file.c_str(), "r")) != NULL) {
		fclose(tmp);
		WcaLog(LOGMSG_STANDARD, "Config file %s already exists, skipping creation", target_file.c_str());
	} else {
		FILE *of = fopen(target_file.c_str(), "w+");
		if (!of) {
			WcaLog(LOGMSG_STANDARD, "Could not create config file %s: %s", target_file.c_str(), strerror(errno));
			return ERROR_INSTALL_FAILURE;
		}	else {
			if (fwrite(buffer, 1, strlen(buffer), of) < 0) {
				WcaLog(LOGMSG_STANDARD, "Error writing config file %s: %s", target_file.c_str(), strerror(errno));
				// ignore the error
			} else {
				WcaLog(LOGMSG_STANDARD, "Wrote config file %s", target_file.c_str());
			}
			fclose(of);
		}
	}
	free(buffer);
  return 0;
}


UINT create_config_file(MSIHANDLE hInstall) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, "InstallDefaultConfigFile");
  ExitOnFailure(hr, "Failed to initialize config updater");

  WcaLog(LOGMSG_STANDARD, "Initialized config updater.");

  char custom_data[1024*2];
  DWORD custom_data_size = sizeof(custom_data);
  if (MsiGetPropertyA(hInstall, "CustomActionData", custom_data, &custom_data_size) == ERROR_SUCCESS) {
		char *ptr = NULL;
		WcaLog(LOGMSG_STANDARD, "%s", custom_data);
		char *p = strtok_s(custom_data, ";", &ptr);
	  char *install_dir = NULL;
	  char *data_dir = NULL;
		while (p)
		{
			char *sep = strchr(p, '=');
			if (strncmp(p, "INSTALL", strlen("INSTALL")) == 0) {
				install_dir = sep+1;
				if (*(sep-1) == '\\')
					*(sep-1) = 0; // trim trailing backslash
			}	else if (strncmp(p, "DATA", strlen("DATA")) == 0)	{
				data_dir = sep+1;
				if (*(sep-1) == '\\')
					*(sep-1) = 0; // trim trailing backslash
			}
			p = strtok_s(NULL, ";", &ptr);
		}

    if (!install_dir)	{
      WcaLog(LOGMSG_STANDARD, "Could not determine Install directory");
      er = ERROR_INSTALL_FAILURE;
    } else if (!data_dir)	{
			WcaLog(LOGMSG_STANDARD, "Could not determine ProgramData directory");
			er = ERROR_INSTALL_FAILURE;
		}	else {
			// normalize paths to use / as separator
			for (char *c = install_dir; *c; ++c) {
				if (*c == '\\')
					*c = '/';
			}
			for (char *c = data_dir; *c; ++c) {
				if (*c == '\\')
					*c = '/';
			}
      int rc = install_config_file(install_dir, data_dir);
      if (rc < 0)
        er = ERROR_INSTALL_FAILURE;
    }
  }	else {
		WcaLog(LOGMSG_STANDARD, "Could not CustomActionData");
  	er = ERROR_INSTALL_FAILURE;
  }
LExit:
  return WcaFinalize(er);
}


UINT __stdcall InstallDefaultConfigFile(MSIHANDLE hInstall) {

	return create_config_file(hInstall);
}


UINT __stdcall RunPostInstall(MSIHANDLE hInstall) {
	return 0;
}


UINT DoInstallService(MSIHANDLE hInstall, char *install_dir, char *data_dir)
{
  char szFilePath[_MAX_PATH];
  int startType = 0;
  LPCSTR szInternNameA = "MySQLRouter";
  LPCTSTR szInternName = TEXT("MySQLRouter");
  LPCSTR szDisplayName = "MySQL Router";
  LPCSTR szFullPath = "";
  LPCSTR szAccountName = "NT AUTHORITY\\LocalService";

  strcpy(szFilePath, "\"");
  strcat(szFilePath, install_dir);
  strcat(szFilePath, "bin\\mysqlrouter.exe\"");
  strcat(szFilePath, " -c \"");
  strcat(szFilePath, data_dir);
  strcat(szFilePath, "\\mysqlrouter.conf\" --service");
  int len = strlen(szFilePath);
  for (int i = 0; i < len; i++)
  {
    if (szFilePath[i] == '/')
    {
      szFilePath[i] = '\\';
    }
  }
  
  NTService service;
  if (!service.SeekStatus(szInternNameA, 1))
  {
    printf("Service already installed\n");
    WcaLog(LOGMSG_STANDARD, "Service already installed, trying to remove...\n");
    stop_service(szInternNameA);
    if (remove_service(szInternName)) {
      WcaLog(LOGMSG_STANDARD, "Error when removing the already installed service\n");
      return FALSE;
    }
    else
      WcaLog(LOGMSG_STANDARD, "Service removed successfully.\n");
  }
  BOOL res = service.Install(startType, szInternNameA, szDisplayName, szFilePath, szAccountName);
  return res;
}

UINT __stdcall InstallService(MSIHANDLE hInstall)
{
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, "InstallService");
  ExitOnFailure(hr, "Failed to initialize service installer");

  WcaLog(LOGMSG_STANDARD, "Initialized service installer.");

  char custom_data[1024 * 2];
  DWORD custom_data_size = sizeof(custom_data);
  if (MsiGetPropertyA(hInstall, "CustomActionData", custom_data, &custom_data_size) == ERROR_SUCCESS) {
    char *ptr = NULL;
    WcaLog(LOGMSG_STANDARD, "%s", custom_data);
    char *p = strtok_s(custom_data, ";", &ptr);
    char *install_dir = NULL;
    char *data_dir = NULL;
    while (p)
    {
      char *sep = strchr(p, '=');
      if (strncmp(p, "INSTALL", strlen("INSTALL")) == 0) {
        install_dir = sep + 1;
        if (*(sep - 1) == '\\')
          *(sep - 1) = 0; // trim trailing backslash
      }
      else if (strncmp(p, "DATA", strlen("DATA")) == 0) {
        data_dir = sep + 1;
        if (*(sep - 1) == '\\')
          *(sep - 1) = 0; // trim trailing backslash
      }
      p = strtok_s(NULL, ";", &ptr);
    }

    if (!install_dir) {
      WcaLog(LOGMSG_STANDARD, "Could not determine Install directory");
      er = ERROR_INSTALL_FAILURE;
    }
    else if (!data_dir) {
      WcaLog(LOGMSG_STANDARD, "Could not determine ProgramData directory");
      er = ERROR_INSTALL_FAILURE;
    }
    else {
      // normalize paths to use / as separator
      for (char *c = install_dir; *c; ++c) {
        if (*c == '\\')
          *c = '/';
      }
      for (char *c = data_dir; *c; ++c) {
        if (*c == '\\')
          *c = '/';
      }
      int rc = DoInstallService(hInstall, install_dir, data_dir);
      if (!rc)
        er = ERROR_INSTALL_FAILURE;
    }
  }
  else {
    WcaLog(LOGMSG_STANDARD, "Could not get CustomActionData");
    er = ERROR_INSTALL_FAILURE;
  }
LExit:
  return WcaFinalize(er);
}


/* DllMain - Initialize and cleanup WiX custom action utils */
extern "C" BOOL WINAPI DllMain(
	__in HINSTANCE hInst,
	__in ULONG ulReason,
	__in LPVOID
	) {
	switch(ulReason) {
	case DLL_PROCESS_ATTACH:
		WcaGlobalInitialize(hInst);
		break;

	case DLL_PROCESS_DETACH:
		WcaGlobalFinalize();
		break;
	}
	return TRUE;
}
