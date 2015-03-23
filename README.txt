MySQL Router 1.0
================

This is a release of MySQL Router, part of MySQL Fabric 1.6.

For the avoidance of doubt, this particular copy of the software
is released under the version 2 of the GNU General Public License.
MySQL Router is brought to you by Oracle.

Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.


Running Tests
-------------

All tests can be run doing the following:

    shell> mkdir build
    shell> cd build
    shell> cmake ..
    shell> make
    shell> make test

After running `make`, test executables and scripts are available
in the folder `tests/bin`. You can directly execute these tests
like this:

    shell> ./tests/bin/tools/boost_libs
    shell> python -B tests/bin/tools/python_exec.py


Documentation
-------------

For further information about MySQL Router or additional
documentation, see:

* The latest information about MySQL: http://www.mysql.com
* The current MySQL Router documentation: http://dev.mysql.com/doc

You can browse the MySQL Reference Manual online or download it
in any of several formats at the URL given earlier in this file.
Source distributions include a local copy of the manual in the
Docs directory.


License
-------

License information can be found in the License.txt file.

MySQL FOSS License Exception
We want free and open source software applications under certain
licenses to be able to use specified GPL-licensed MySQL client
libraries despite the fact that not all such FOSS licenses are
compatible with version 2 of the GNU General Public License.
Therefore there are special exceptions to the terms and conditions
of the GPLv2 as applied to these client libraries, which are
identified and described in more detail in the FOSS License
Exception at
<http://www.mysql.com/about/legal/licensing/foss-exception.html>.

This distribution may include materials developed by third
parties. For license and attribution notices for these
materials, please refer to the documentation that accompanies
this distribution (see the "Licenses for Third-Party Components"
appendix) or view the online documentation at
<http://dev.mysql.com/doc/>.

GPLv2 Disclaimer
For the avoidance of doubt, except that if any license choice
other than GPL or LGPL is available it will apply instead,
Oracle elects to use only the General Public License version 2
(GPLv2) at this time for any software where a choice of GPL
license versions is made available with the language indicating
that GPLv2 or any later version may be used, or where a choice
of which version of the GPL is applied is otherwise unspecified.
