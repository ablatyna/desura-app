/*
Copyright (C) 2014 Bad Juju Games, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.

Contact us at legal@badjuju.com.

*/


// interface: void copyFolder(Path src, Path dest, std::vector<std::string> *vIgnoreList = nullptr, bool copyOverExisting = true);
//            void copyFolder(std::string src, std::string dest, std::vector<std::string> *vIgnoreList = nullptr, bool copyOverExisting = true)

// set up test env for util_fs testing
#define TEST_DIR "copyFolder"
#include "util_fs/testFunctions.cpp"

#include "Common.h"
#include "util/UtilFs.h"
#include "util/UtilFsPath.h"
using namespace UTIL::FS;

#define SRC (getTestDirectory()/"0")
#define DES1 (getTestDirectory()/"1")
#define DES2 (getTestDirectory()/"2")
#define DES3 (getTestDirectory()/"3")
#define DES4 (getTestDirectory()/"4")
#define SRC5 (getTestDirectory()/"S5")
#define DES5 (getTestDirectory()/"D5")

namespace UnitTest
{

	TEST_F(FSTestFixture, copyFolder_Path_NULL_default)
	{
		Path src(SRC.string(), "", false);
		Path des(DES1.string(), "", false);

		ASSERT_TRUE(!fs::exists(DES1));
		copyFolder(src, des);
		ASSERT_TRUE(fs::exists(DES1));
		ASSERT_EQ_FILES(SRC / "0", DES1 / "0");
		ASSERT_EQ_FILES(SRC / "1.txt", DES1 / "1.txt");
		ASSERT_EQ_FILES(SRC / "2.png", DES1 / "2.png");
		ASSERT_EQ_FILES(SRC / UNICODE_EXAMPLE_FILE, DES1 / UNICODE_EXAMPLE_FILE);
	}

	TEST_F(FSTestFixture, copyFolder_Path_vector_default)
	{
		Path src(SRC.string(), "", false);
		Path des(DES2.string(), "", false);
		std::vector<std::string> ignoreList;
		ignoreList.push_back("1.txt");

		ASSERT_TRUE(!fs::exists(DES2));
		copyFolder(src, des, &ignoreList);
		ASSERT_TRUE(fs::exists(DES2));
		ASSERT_EQ_FILES(SRC / "0", DES2 / "0");
		ASSERT_TRUE(!fs::exists(DES2 / "1.txt"));
		ASSERT_EQ_FILES(SRC / "2.png", DES2 / "2.png");
		ASSERT_EQ_FILES(SRC / UNICODE_EXAMPLE_FILE, DES2 / UNICODE_EXAMPLE_FILE);
	}

	TEST_F(FSTestFixture, copyFolder_string_NULL_default)
	{
		ASSERT_TRUE(!fs::exists(DES3));
		copyFolder(SRC.string(), DES3.string());
		ASSERT_TRUE(fs::exists(DES3));
		ASSERT_EQ_FILES(SRC / "0", DES3 / "0");
		ASSERT_EQ_FILES(SRC / "1.txt", DES3 / "1.txt");
		ASSERT_EQ_FILES(SRC / "2.png", DES3 / "2.png");
		ASSERT_EQ_FILES(SRC / UNICODE_EXAMPLE_FILE, DES3 / UNICODE_EXAMPLE_FILE);
	}

	TEST_F(FSTestFixture, copyFolder_string_vector_default)
	{
		std::vector<std::string> ignoreList;
		ignoreList.push_back("1.txt");

		ASSERT_TRUE(!fs::exists(DES4));
		copyFolder(SRC.string(), DES4.string(), &ignoreList);
		ASSERT_TRUE(fs::exists(DES4));
		ASSERT_EQ_FILES(SRC / "0", DES4 / "0");
		ASSERT_TRUE(!fs::exists(DES4 / "1.txt"));
		ASSERT_EQ_FILES(SRC / "2.png", DES4 / "2.png");
		ASSERT_EQ_FILES(SRC / UNICODE_EXAMPLE_FILE, DES4 / UNICODE_EXAMPLE_FILE);
	}

	TEST_F(FSTestFixture, copyFolder_string_rec)
	{
		// here we copy a directory with files into a directory ...
		copyFolder(SRC.string(), (SRC5 / "0").string());

		ASSERT_TRUE(!fs::exists(DES5));
		// ... so we can copy the directory with the directory with files ;)
		copyFolder(SRC5.string(), DES5.string());
		ASSERT_TRUE(fs::exists(DES5));
		ASSERT_TRUE(fs::exists(DES5 / "0"));
		ASSERT_EQ_FILES(SRC / "0", DES5 / "0" / "0");
		ASSERT_EQ_FILES(SRC / "1.txt", DES5 / "0" / "1.txt");
		ASSERT_EQ_FILES(SRC / "2.png", DES5 / "0" / "2.png");
		ASSERT_EQ_FILES(SRC / UNICODE_EXAMPLE_FILE, DES5 / "0" / UNICODE_EXAMPLE_FILE);
	}
}

