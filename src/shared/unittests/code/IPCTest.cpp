/*
$LicenseInfo:firstyear=2014&license=lgpl$
Copyright (C) 2014, Linden Research, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
version 2.1 of the License only.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, see <http://www.gnu.org/licenses/>
or write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
$/LicenseInfo$
*/
#include <gtest/gtest.h>

#include "Common.h"
#include "IPCClass.h"
#include "IPCParameterI.h"

using namespace IPC;
using namespace testing;


#ifdef LINK_WITH_GMOCK
#include <gmock/gmock.h>

class IPCFixture : public ::testing::Test
{
public:
	IPCFixture()
	{
		EXPECT_CALL(*this, voidFunct()).Times(0);
		EXPECT_CALL(*this, voidFunctOne(_)).Times(0);
		EXPECT_CALL(*this, voidFunctTwo(_, _)).Times(0);
		EXPECT_CALL(*this, normFunct()).Times(0);
		EXPECT_CALL(*this, normFunctOne(_)).Times(0);
		EXPECT_CALL(*this, normFunctTwo(_, _)).Times(0);
		EXPECT_CALL(*this, setCrashSettings(_, _)).Times(0);
	}

	~IPCFixture()
	{
		safe_delete(m_vParameters);
	}

	MOCK_METHOD0(voidFunct, void());
	MOCK_METHOD1(voidFunctOne, void(int));
	MOCK_METHOD2(voidFunctTwo, void(const char*, int));
	MOCK_METHOD0(normFunct, int());
	MOCK_METHOD1(normFunctOne, char*(int));
	MOCK_METHOD2(normFunctTwo, bool(const char*, int));
	MOCK_METHOD2(setCrashSettings, void(bool, const char*));

	template <typename T>
	void addParameter(T t)
	{
		m_vParameters.push_back(newParameterS(t));
	}

	std::unique_ptr<IPCParameterI> call(NetworkFunctionI* &pFunct)
	{
		uint32 nSize = 0;
		char* szBuff = IPC::serializeList(m_vParameters, nSize);

		auto ret = std::unique_ptr<IPCParameterI>(pFunct->call(szBuff, nSize, m_vParameters.size()));

		safe_delete(m_vParameters);
		safe_delete(szBuff);
		safe_delete(pFunct);

		return ret;
	}

	std::vector<IPCParameterI*> m_vParameters;
};

TEST_F(IPCFixture, voidFunct)
{
	EXPECT_CALL(*this, voidFunct()).Times(1);

	auto pFunct = networkFunctionV<IPCFixture>(this, &IPCFixture::voidFunct);
	auto pRet = std::unique_ptr<IPCParameterI>(pFunct->call(nullptr, 0, 0));
	ASSERT_EQ(IPC::getType<void>(), pRet->getType());
}

TEST_F(IPCFixture, voidFunctOne)
{
	EXPECT_CALL(*this, voidFunctOne(Eq(1234))).Times(1);

	auto pFunct = networkFunctionV<IPCFixture>(this, &IPCFixture::voidFunctOne);
	addParameter<int>(1234);

	auto pRet = call(pFunct);
	ASSERT_EQ(IPC::getType<void>(), pRet->getType());
}

TEST_F(IPCFixture, voidFunctTwo)
{
	EXPECT_CALL(*this, voidFunctTwo(StrEq("apple"), Eq(6789))).Times(1);

	auto pFunct = networkFunctionV<IPCFixture>(this, &IPCFixture::voidFunctTwo);

	addParameter<char*>("apple");
	addParameter<int>(6789);

	auto pRet = call(pFunct);
	ASSERT_EQ(IPC::getType<void>(), pRet->getType());
}

TEST_F(IPCFixture, setCrashSettings)
{
	EXPECT_CALL(*this, setCrashSettings(Eq(true), StrEq("lodle"))).Times(1);

	auto pFunct = networkFunctionV<IPCFixture>(this, &IPCFixture::setCrashSettings);

	addParameter<bool>(true);
	addParameter<char*>("lodle");

	auto pRet = call(pFunct);
	ASSERT_EQ(IPC::getType<void>(), pRet->getType());
}


TEST_F(IPCFixture, normFunct)
{
	EXPECT_CALL(*this, normFunct()).Times(1).WillOnce(Return(345));

	auto pFunct = networkFunction<IPCFixture>(this, &IPCFixture::normFunct);
	auto pRet = std::unique_ptr<IPCParameterI>(pFunct->call(nullptr, 0, 0));

	ASSERT_EQ(IPC::getType<int>(), pRet->getType());
	ASSERT_EQ(345, getParameterValue<int>(pRet.get()));
}

TEST_F(IPCFixture, normFunctOne)
{
	EXPECT_CALL(*this, normFunctOne(Eq(1234))).Times(1).WillOnce(Invoke([](int){
		char* ret = new char[8];
		Safe::strcpy(ret, 8, "banana");
		return ret;
	}));

	auto pFunct = networkFunction<IPCFixture>(this, &IPCFixture::normFunctOne);

	addParameter<int>(1234);

	auto pRet = call(pFunct);

	ASSERT_EQ(IPC::getType<char*>(), pRet->getType());
	ASSERT_STREQ("banana", getParameterValue<char*>(pRet.get()));
}

TEST_F(IPCFixture, normFunctTwo)
{
	EXPECT_CALL(*this, normFunctTwo(StrEq("cherry"), Eq(6789))).Times(1).WillOnce(Return(true));
	auto pFunct = networkFunction<IPCFixture>(this, &IPCFixture::normFunctTwo);

	addParameter<char*>("cherry");
	addParameter<int>(6789);

	auto pRet = call(pFunct);

	ASSERT_EQ(IPC::getType<bool>(), pRet->getType());
	ASSERT_EQ(true, getParameterValue<bool>(pRet.get()));
}



class IPCEventFixture : public ::testing::Test
{
public:
	IPCEventFixture()
	{
		onUint32Event += delegate(this, &IPCEventFixture::onUint32EventCB);
		onVoidEvent += delegate(this, &IPCEventFixture::onVoidEventCB);
	}

	MOCK_METHOD1(onUint32EventCB, void(uint32&));
	MOCK_METHOD0(onVoidEventCB, void());

	Event<uint32> onUint32Event;
	EventV onVoidEvent;
};


TEST_F(IPCEventFixture, eventV)
{
	EXPECT_CALL(*this, onVoidEventCB()).Times(1);

	IPC::IPCEventV e(&onVoidEvent);

	std::vector<IPCParameterI*> vParams;
	e.trigger(vParams);
}

TEST_F(IPCEventFixture, eventUInt32)
{
	EXPECT_CALL(*this, onUint32EventCB(Eq(123))).Times(1);

	IPC::IPCEvent<uint32> e(&onUint32Event);

	std::vector<IPCParameterI*> vParams;
	vParams.push_back(newParameterS(123));

	e.trigger(vParams);
	safe_delete(vParams);
}


#include "IPCManager.h"

class IPCClassFixture : public ::testing::Test, protected IPCClass, protected IPCManagerI
{
public:
	IPCClassFixture()
		: IPCClass(this, 0, DesuraId())
	{
		registerEvent(IPC::IPCEventHandle(&uint32EventCB), "uint32Event"); //REG_EVENT(uint32Event);
		registerEvent(new IPC::IPCEventV(&voidEventCB), "voidEvent"); //REG_EVENT_VOID(voidEvent);
		
		LINK_EVENT(uint32Event, uint32);
		LINK_EVENT_VOID(voidEvent);

		REG_FUNCTION(IPCClassFixture, uint32FunctionCB);
		REG_FUNCTION_VOID(IPCClassFixture, voidFunctionCB);

		uint32EventCB += delegate(this, &IPCClassFixture::onUint32EventCB);
		voidEventCB += delegate(this, &IPCClassFixture::onVoidEventCB);
	}

	Event<uint32> uint32Event;
	EventV voidEvent;

	Event<uint32> uint32EventCB;
	EventV voidEventCB;

	MOCK_METHOD1(onUint32EventCB, void(uint32&));
	MOCK_METHOD0(onVoidEventCB, void());

	MOCK_METHOD0(uint32FunctionCB, uint32());
	MOCK_METHOD0(voidFunctionCB, void());


	uint32 callUint32Function()
	{
		return IPC::functionCall<uint32>(this, "uint32FunctionCB");
	}

	void callVoidFunction()
	{
		IPC::functionCallV(this, "voidFunctionCB");
	}

	void sendMessage(const char* buff, uint32 size, uint32 id, uint8 type) override
	{
		messageRecived(type, buff, size);
	}

	void sendLoopbackMessage(const char* buff, uint32 size, uint32 id, uint8 type) override
	{
		messageRecived(type, buff, size);
	}

	void destroyClass(IPCClass* obj) override
	{
	}

	bool isDisconnected() override
	{
		return false;
	}

	WeakPtr<IPCClass> createClass(const char* name) override
	{
		return WeakPtr<IPCClass>();
	}
};

TEST_F(IPCClassFixture, FullUint32Func)
{
	EXPECT_CALL(*this, uint32FunctionCB()).Times(1).WillOnce(Return(123));
	EXPECT_EQ(123, callUint32Function());
}

TEST_F(IPCClassFixture, FullVoidFunc)
{
	EXPECT_CALL(*this, voidFunctionCB()).Times(1);
	callVoidFunction();
}

TEST_F(IPCClassFixture, FullUint32Event)
{
	EXPECT_CALL(*this, onUint32EventCB(Eq(123))).Times(1);
	uint32 t = 123;
	uint32Event(t);
}

TEST_F(IPCClassFixture, FullVoidEvent)
{
	EXPECT_CALL(*this, onVoidEventCB()).Times(1);
	voidEvent();
}


#endif



#include <limits>

template <typename Param, typename PType>
class IPCParameterFixture : public ::testing::TestWithParam<PType>
{
public:
	void run()
	{
		uint32 nSize = 0;
		gcBuff szBuff(1);

		{
			Param p(GetParam());

			nSize = p.getSerializeSize();
			szBuff = gcBuff(nSize);

			p.serialize(szBuff);
		}

		{
			Param p;
			uint32 nUsed = p.deserialize(szBuff, nSize);

			ASSERT_EQ(nUsed, nSize);
			AssertEq<PType>(getParameterValue<PType>(&p, false));
		}
	}

	template <typename U>
	void AssertEq(U val)
	{
		ASSERT_EQ(GetParam(), val);
	}

	template <>
	void AssertEq<const char*>(const char* val)
	{
		ASSERT_STREQ(GetParam(), val);
	}

	template <>
	void AssertEq<double>(double val)
	{
		ASSERT_DOUBLE_EQ(GetParam(), val);
	}
};

typedef IPCParameterFixture<PUint32, uint32> ParamTestUint32;
typedef IPCParameterFixture<PInt32, int32> ParamTestInt32;
typedef IPCParameterFixture<PBool, bool> ParamTestBool;

typedef IPCParameterFixture<PUint64, uint64> ParamTestUint64;
typedef IPCParameterFixture<PDouble, double> ParamTestDouble;
typedef IPCParameterFixture<PString, const char*> ParamTestString;
typedef IPCParameterFixture<PException, gcException> ParamTestException;


TEST_P(ParamTestUint32, PUint32)
{
	run();
}

TEST_P(ParamTestInt32, PInt32)
{
	run();
}

TEST_P(ParamTestBool, PBool)
{
	run();
}

TEST_P(ParamTestUint64, PUint64)
{
	run();
}

TEST_P(ParamTestDouble, PDouble)
{
	run();
}

TEST_P(ParamTestString, PString)
{
	run();
}


INSTANTIATE_TEST_CASE_P(uint32_perm, ParamTestUint32, ::testing::Values(0, -1, 1, UINT_MAX));
INSTANTIATE_TEST_CASE_P(int32_perm, ParamTestInt32, ::testing::Values(0, -1, 1, INT_MAX, INT_MIN));
INSTANTIATE_TEST_CASE_P(bool_perm, ParamTestBool, ::testing::Values(true, false));

INSTANTIATE_TEST_CASE_P(uint64_perm, ParamTestUint64, ::testing::Values(0, -1, 1, std::numeric_limits<uint64>::max()));
INSTANTIATE_TEST_CASE_P(double_perm, ParamTestDouble, ::testing::Values(0, -1.0, 1.0, 0.0001, -0.0001, std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity()));
INSTANTIATE_TEST_CASE_P(string_perm, ParamTestString, ::testing::Values(nullptr, "", "a string", "a very long string !!", "\n\t"));






