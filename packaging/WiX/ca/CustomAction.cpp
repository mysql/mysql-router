/* Copyright (c) 2010, 2016 Oracle and/or its affiliates. All rights reserved.

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

/*
 * Search the registry for a service whose ImagePath starts
 * with our install directory. Stop and remove it if requested.
 */
static TCHAR last_service_name[128];
int remove_service(TCHAR *installdir, int check_only) {
	HKEY hKey;
	int done = 0;

	if(wcslen(installdir) < 3) {
		WcaLog(LOGMSG_STANDARD, "INSTALLDIR is suspiciously short, better not do anything.");
		return 0;
	}

	if(check_only == 0) {
		WcaLog(LOGMSG_STANDARD, "Determining number of matching services...");
		int servicecount = remove_service(installdir, 1);
		if(servicecount <= 0) {
			WcaLog(LOGMSG_STANDARD, "No services found, not removing anything.");
			return 0;
		} else if(servicecount == 1) {
			TCHAR buf[256];
			swprintf_s(buf, sizeof(buf), TEXT("There is a service called '%ls' set up to run from this installation. Do you wish me to stop and remove that service?"), last_service_name);
			int rc = MessageBox(NULL, buf, TEXT("Removing MySQL Router"), MB_ICONQUESTION|MB_YESNOCANCEL|MB_SYSTEMMODAL);
			if(rc == IDCANCEL) return -1;
			if(rc != IDYES) return 0;
		} else if(servicecount > 0) {
			TCHAR buf[256];
			swprintf_s(buf, sizeof(buf), TEXT("There appear to be %d services set up to run from this installation. Do you wish me to stop and remove those services?"), servicecount);
			int rc = MessageBox(NULL, buf, TEXT("Removing MySQL Router"), MB_ICONQUESTION|MB_YESNOCANCEL|MB_SYSTEMMODAL);
			if(rc == IDCANCEL) return -1;
			if(rc != IDYES) return 0;
		}
	}

	if(check_only == -1) check_only = 0;

	WcaLog(LOGMSG_STANDARD, "Looking for service...");
	WcaLog(LOGMSG_STANDARD, "INSTALLDIR = %ls", installdir);
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\services"), 0, KEY_READ, &hKey)==ERROR_SUCCESS) {
		DWORD index = 0;
		TCHAR keyname[1024];
		DWORD keylen = sizeof(keyname);
		FILETIME t;
		/* Go through all services in the registry */
		while(RegEnumKeyExW(hKey, index, keyname, &keylen, NULL, NULL, NULL, &t) == ERROR_SUCCESS) {
			HKEY hServiceKey = 0;
			TCHAR path[1024];
			DWORD pathlen = sizeof(path)-1;
			if (RegOpenKeyExW(hKey, keyname, NULL, KEY_READ, &hServiceKey) == ERROR_SUCCESS) {
				/* Look at the ImagePath value of each service */
				if (RegQueryValueExW(hServiceKey, TEXT("ImagePath"), NULL, NULL, (LPBYTE)path, &pathlen) == ERROR_SUCCESS) {
					path[pathlen] = 0;
					TCHAR *p = path;
					if(p[0] == '"') p += 1;
					/* See if it is similar to our install directory */
					if(wcsncmp(p, installdir, wcslen(installdir)) == 0) {
						WcaLog(LOGMSG_STANDARD, "Found service '%ls' with ImagePath '%ls'.", keyname, path);
						swprintf_s(last_service_name, sizeof(last_service_name), TEXT("%ls"), keyname);
						/* If we are supposed to stop and remove the service... */
						if(!check_only) {
							WcaLog(LOGMSG_STANDARD, "Trying to stop the service.");
							SC_HANDLE hSCM = NULL;
							hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
							if(hSCM != NULL) {
								SC_HANDLE hService = NULL;
								hService = OpenService(hSCM, keyname, SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE);
								if(hService != NULL) {
									WcaLog(LOGMSG_STANDARD, "Waiting for the service to stop...");
									SERVICE_STATUS status;
									/* Attempt to stop the service */
									if(ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
										/* Now wait until it's stopped */
										while("it's one big, mean and cruel world out there") {
											if(!QueryServiceStatus(hService, &status)) break;
											if(status.dwCurrentState == SERVICE_STOPPED) break;
											Sleep(1000);
										}
										WcaLog(LOGMSG_STANDARD, "Stopped the service.");
									}
									/* Mark the service for deletion */
									DeleteService(hService);
									CloseServiceHandle(hService);
								}
								CloseServiceHandle(hSCM);
							}
						}
						done++;
					}
				}
				RegCloseKey(hServiceKey);
			}
			index++;
			keylen = sizeof(keyname)-1;
		}
		RegCloseKey(hKey);
	} else {
		WcaLog(LOGMSG_STANDARD, "Can't seem to go through the list of installed services in the registry.");
	}
	return done;
}


UINT wrap(MSIHANDLE hInstall, char *name, int check_only) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, name);
  ExitOnFailure(hr, "Failed to initialize");

  WcaLog(LOGMSG_STANDARD, "Initialized.");

  TCHAR INSTALLDIR[1024];
  DWORD INSTALLDIR_size = sizeof(INSTALLDIR);
  if (MsiGetPropertyW(hInstall, TEXT("CustomActionData"), INSTALLDIR, &INSTALLDIR_size) == ERROR_SUCCESS) {
		int rc = remove_service(INSTALLDIR, check_only);
		if (rc < 0) {
		  er = ERROR_CANCELLED;
		}
  } else {
		er = ERROR_CANT_ACCESS_FILE;
  }

LExit:
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
		if (length_ + length + 1 < size_)	{
			char *tmp = (char*)realloc(buffer_, size_ + length + 1);
			size_ += length + 1;
			if (!tmp)	{
				free(buffer_);
				buffer_ = NULL;
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

/* Copies the default config file from the install dir's etc folder to the proper place in
  ProgramData */
int install_config_file(const char *install_dir, const char* progdata_dir) {
  if (!CreateDirectoryA(progdata_dir, 0)) {
    int er;
    if ((er = GetLastError()) != ERROR_ALREADY_EXISTS) {
      WcaLog(LOGMSG_STANDARD, "Can't create configuration directory: %s: %s", progdata_dir, er);
      return -1;
    }
  }
	StringBuffer source_path;
	source_path.append(install_dir);
	source_path.append("\\etc\\mysqlrouter.ini.sample");
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
	target_file.append("\\mysqlrouter.ini");
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
