/*
Copyright (C) 2011 Mark Chandler (Desura Net Pty Ltd)
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

#include "Common.h"
#include "CVarManager.h"
#include "sqlite3x.hpp"

#ifdef WIN32
	#include <shlobj.h>
#endif

CVarManager* g_pCVarMang = nullptr;
CVarRegTargetI* g_pCVarRegTarget = nullptr;

void InitCVarManger()
{
	if (!g_pCVarMang)
	{
		g_pCVarMang = new CVarManager();
		g_pCVarRegTarget = g_pCVarMang;
	}

	g_pCVarMang->loadNormal();
	g_pCVarMang->loadWinUser();
}

void DestroyCVarManager()
{
	SaveCVars();
	safe_delete(g_pCVarMang);
}

void SaveCVars()
{
	g_pCVarMang->saveAll();
}



#define CREATE_CVARUSER "CREATE TABLE cvaruser(user TEXT, name TEXT, value TEXT, PRIMARY KEY (user, name));"
#define COUNT_CVARUSER "SELECT count(*) FROM sqlite_master WHERE name='cvaruser';"

#define CREATE_CVARWIN "CREATE TABLE cvarwin(user TEXT, name TEXT, value TEXT, PRIMARY KEY (user, name));"
#define COUNT_CVARWIN "SELECT count(*) FROM sqlite_master WHERE name='cvarwin';"

#define CREATE_CVAR "CREATE TABLE cvar(name TEXT PRIMARY KEY, value TEXT);"
#define COUNT_CVAR "SELECT count(*) FROM sqlite_master WHERE name='cvar';"


CVarManager::CVarManager()
	: BaseManager()
	, m_szCVarDb(UTIL::OS::getAppDataPath(L"settings_b.sqlite"))
{
	UTIL::FS::recMakeFolder(UTIL::FS::Path(m_szCVarDb, "", true));

	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());

		if (db.executeint(COUNT_CVARUSER) == 0)
			db.executenonquery(CREATE_CVARUSER);

		if (db.executeint(COUNT_CVARWIN) == 0)
			db.executenonquery(CREATE_CVARWIN);

		if (db.executeint(COUNT_CVAR) == 0)
			db.executenonquery(CREATE_CVAR);
	}
	catch (std::exception &e)
	{
		Warning("Failed to create cvar tables: {0}\n", e.what());
	}
}

CVarManager::~CVarManager()
{
	std::vector<gcRefPtr<CVar>> vList;
	getCVarList(vList);

	for (auto pVar : vList)
		pVar->deregister();
}

bool CVarManager::RegCVar(const gcRefPtr<CVar> &var)
{
	auto temp = findItem(var->getName());

	if (temp)
		return false;

	addItem(var);

	if ((var->getFlags() & CFLAG_USER) && m_bUserLoaded)
		loadUser(var.get());
	else if ((var->getFlags() & CFLAG_WINUSER) && m_bWinUserLoaded)
		loadWinUser(var.get());
	else if (m_bNormalLoaded)
		loadNormal(var.get());

	return true;
}

//if this screws up its too late any way
void  CVarManager::UnRegCVar(const gcRefPtr<CVar> &var)
{
	removeItem(var->getName());
}


void CVarManager::loadUser(CVar* var)
{
	loadCVarFromDb(var, "SELECT value FROM cvaruser WHERE name=? AND user=?;", gcString("{0}", m_uiUserId));
}

void CVarManager::loadWinUser(CVar* var)
{
	loadCVarFromDb(var, "SELECT value FROM cvarwin WHERE name=? AND user=?;", getWinUser());
}

void CVarManager::loadNormal(CVar* var)
{
	loadCVarFromDb(var, "SELECT value FROM cvar WHERE name=?;", "");
}

void CVarManager::loadCVarFromDb(CVar *var, const char* szSql, gcString strExtra)
{
	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());
		sqlite3x::sqlite3_command cmd(db, szSql);
		cmd.bind(1, var->getName());
		cmd.bind(2, strExtra);

		sqlite3x::sqlite3_reader reader = cmd.executereader();

		if (reader.read())
		{
			std::string value = reader.getstring(0);
			var->setValueOveride(value.c_str(), true);
		}
	}
	catch (std::exception &)
	{
	}
}

std::wstring CVarManager::getWinUser()
{
	wchar_t username[255] = {0};
#ifdef WIN32
	DWORD size = 255;
	GetUserNameW(username, &size);
#endif
	return username;
}





void CVarManager::loadUser(uint32 userid)
{
	m_uiUserId = userid;
	m_bUserLoaded = true;

	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());
		sqlite3x::sqlite3_command cmd(db, "SELECT name, value FROM cvaruser WHERE user=?;");
		cmd.bind(1, (int)m_uiUserId);

		sqlite3x::sqlite3_reader cmdResults = cmd.executereader();
		loadFromDb(cmdResults);
	}
	catch (std::exception &)
	{
	}
}

void CVarManager::loadWinUser()
{
	m_bWinUserLoaded = true;

	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());
		sqlite3x::sqlite3_command cmd(db, "SELECT name, value FROM cvarwin WHERE user=?;");

		cmd.bind(1, getWinUser());

		sqlite3x::sqlite3_reader cmdResults = cmd.executereader();
		loadFromDb(cmdResults);
	}
	catch (std::exception &e)
	{
		Warning("Failed to load cvar win user: {0}\n", e.what());
	}
}

void CVarManager::loadNormal()
{
	m_bNormalLoaded = true;

	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());
		sqlite3x::sqlite3_command cmd(db, "SELECT name, value FROM cvar;");

		sqlite3x::sqlite3_reader cmdResults = cmd.executereader();
		loadFromDb(cmdResults);
	}
	catch (std::exception &e)
	{
		Warning("Failed to load cvar normal: {0}\n", e.what());
	}
}

void CVarManager::saveAll()
{
	saveUser();
	saveWinUser();
	saveNormal();
}

void CVarManager::saveUser()
{
	if (m_uiUserId == UINT_MAX)
		return;

	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());

		{
			sqlite3x::sqlite3_command cmd(db,"DELETE FROM cvaruser where user=?;");
			cmd.bind(1, (int)m_uiUserId);
			cmd.executenonquery();
		}

		sqlite3x::sqlite3_transaction trans(db);

		{
			sqlite3x::sqlite3_command cmd(db, "INSERT INTO cvaruser (name, value, user) VALUES (?,?,?);");
			cmd.bind(3, (int)m_uiUserId);
			saveToDb(cmd, CFLAG_USER);
		}

		trans.commit();
	}
	catch (std::exception &e)
	{
		Warning("Failed to save cvar user: {0}\n", e.what());
	}
}

void CVarManager::saveWinUser()
{
	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());

		{
			sqlite3x::sqlite3_command cmd(db,"DELETE FROM cvarwin where user=?;");
			cmd.bind(1, getWinUser());
			cmd.executenonquery();
		}

		sqlite3x::sqlite3_command cmd(db, "INSERT INTO cvarwin (name, value, user) VALUES (?,?,?);");
		cmd.bind(3, getWinUser());

		sqlite3x::sqlite3_transaction trans(db);
		saveToDb(cmd, CFLAG_WINUSER);
		trans.commit();
	}
	catch (std::exception &e)
	{
		Warning("Failed to save cvar win user: {0}\n", e.what());
	}
}

void CVarManager::saveNormal()
{
	try
	{
		sqlite3x::sqlite3_connection db(m_szCVarDb.c_str());
		db.executenonquery("DELETE FROM cvar;");

		sqlite3x::sqlite3_command cmd(db, "INSERT INTO cvar (name, value) VALUES (?,?);");

		sqlite3x::sqlite3_transaction trans(db);
		saveToDb(cmd, CFLAG_NOFLAGS);
		trans.commit();
	}
	catch (std::exception &e)
	{
		Warning("Failed to save cvar normal: {0}\n", e.what());
	}
}


void CVarManager::cleanUserCvars()
{
	for (uint32 x=0; x<getCount(); x++)
	{
		auto cvarNode = getItem(x);

		if (cvarNode->getFlags() & CFLAG_USER)
			cvarNode->setDefault();
	}

	m_uiUserId = -1;
}

void CVarManager::loadFromDb(sqlite3x::sqlite3_reader &reader)
{
	while (reader.read())
	{
		std::string name = reader.getstring(0);
		std::string value = reader.getstring(1);

		auto temp = findItem(name.c_str());

		if (temp)
			temp->setValueOveride(value.c_str(), true);
	}
}

void CVarManager::saveToDb(sqlite3x::sqlite3_command &cmd, uint8 flags)
{
	for (uint32 x=0; x<getCount(); x++)
	{
		auto cvarNode = getItem(x);

		if (cvarNode->getFlags() & CFLAG_NOSAVE)
			continue;

		//dont save if user = true and usercvar = false || user = false and usercvar = true
		if ((flags & CFLAG_USER) ^ (cvarNode->getFlags() & CFLAG_USER))
			continue;

		if ((flags & CFLAG_WINUSER) ^ (cvarNode->getFlags() & CFLAG_WINUSER))
			continue;

		//dont save if setting hasnt changed
		if (!cvarNode->getExitString() || (strcmp(cvarNode->getExitString(), cvarNode->getDefault())) == 0)
			continue;

		cmd.bind(1, std::string(cvarNode->getName()));
		cmd.bind(2, std::string(cvarNode->getExitString()));
		cmd.executenonquery();
	}
}

gcRefPtr<CVar> CVarManager::findCVar(const char* name)
{
	return findItem(name);
}

void CVarManager::getCVarList(std::vector<gcRefPtr<CVar>> &vList)
{
	for (uint32 x=0; x<getCount(); x++)
		vList.push_back(getItem(x));
}
