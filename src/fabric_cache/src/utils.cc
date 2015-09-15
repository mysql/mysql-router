/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "utils.h"

#include "mysqlrouter/fabric_cache.h"

#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <vector>

using std::getline;
using std::hex;
using std::stoi;
using std::stringstream;
using std::tm;
using std::vector;

string get_string(const char *input_str) {
  if (input_str == nullptr) {
    return "";
  }
  return string(input_str);
}


int IntegerValueComparator::compare(string val_a, string val_b) {
  if (atoi(val_a.c_str()) > atoi(val_b.c_str())) {
    return 1;
  }
  else if (atoi(val_a.c_str()) < atoi(val_b.c_str())) {
    return -1;
  }
  return 0;
}

time_t DateTimeValueComparator::convert_to_time_t(string datetime_str) {
  stringstream ss(datetime_str);

  char delimiter = ' ';
  string part;

  vector<int> date;
  vector<int> time;

  size_t pos;

  struct tm datetime_tm;

  //Parse the date and time parts separately
  while (getline(ss, part, delimiter)) {
    //Parse Date
    if (date.size() == 0) {
      stringstream ss_date(part);
      while (getline(ss_date, part, '-')) {
        date.push_back(stoi(part));
      }
    }
      //Parse Time
    else if (date.size() == 3) {
      stringstream ss_time(part);
      while (getline(ss_time, part, ':')) {
        time.push_back(stoi(part));
      }
    }
      //Invalid Date Format
    else {
      return 0;
    }
  }

  //Parse Millisecond information
  if ((pos = datetime_str.find('.')) != string::npos) {
    time.push_back(stoi(datetime_str.substr(pos++)));
  }

  auto date_size = date.size();
  for (size_t count = 1; count <= (date_size - 3); count++) {
    date.push_back(0);
  }

  auto time_size = time.size();
  for (size_t count = 1; count <= (time_size - 4); count++) {
    time.push_back(0);
  }

  //Initialize the tm structure with the date.
  datetime_tm.tm_mday = date[0];
  datetime_tm.tm_mon = date[1];
  datetime_tm.tm_year = date[2];

  //Initialize the tm structure with the time.
  //NOTE: Milliseconds are not being used in conversion.
  datetime_tm.tm_hour = time[0];
  datetime_tm.tm_min = time[1];
  datetime_tm.tm_sec = time[2];

  return mktime(&datetime_tm);
}

int DateTimeValueComparator::compare(string val_a, string val_b) {
  double diff = difftime(convert_to_time_t(val_a),
                         convert_to_time_t(val_b));
  if (diff > 0) {
    return 1;
  }
  else if (diff < 0) {
    return -1;
  }
  return 0;
}

int StringValueComparator::compare(string val_a, string val_b) {
  return strcmp(val_a.c_str(), val_b.c_str());
}

int MD5HashValueComparator::convert_hexa_char_to_int(char c) {
  int result;

  std::stringstream ss;
  ss << std::hex << c;
  ss >> result;

  return result;
}

int MD5HashValueComparator::compare(string val_a, string val_b) {
  // A MD5 hash value is 16 bytes. Iterate through the string
  // comparing the value at each position. The earliest mismatch
  // helps us decide which of the values is greater.
  for (size_t i = 0; i <= 15; i++) {
    int a = convert_hexa_char_to_int(val_a.at(i));
    int b = convert_hexa_char_to_int(val_b.at(i));
    if (a > b) {
      return 1;
    }
    else if (a < b) {
      return -1;
    }
    else {
      continue;
    }
  }
  return 0;
}
