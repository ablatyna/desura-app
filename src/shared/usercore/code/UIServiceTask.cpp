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
#include "UIServiceTask.h"

#include "IPCServiceMain.h"
#include "IPCUninstallMcf.h"

using namespace UserCore::ItemTask;


UIServiceTask::UIServiceTask(gcRefPtr<UserCore::Item::ItemHandleI> handle, bool removeAll, bool removeAcc)
	: UIBaseServiceTask(UserCore::Item::ITEM_STAGE::STAGE_UNINSTALL, "UnInstall", handle)
{
	m_bRemoveAll = removeAll;
	m_bRemoveAcc = removeAcc;

	m_pIPCUI = nullptr;
	m_bRunning = false;
}

UIServiceTask::~UIServiceTask()
{
	if (m_bRunning)
		waitForFinish();

	if (m_pIPCUI)
		m_pIPCUI->destroy();

	m_pIPCUI = nullptr;
}

bool UIServiceTask::initService()
{
	gcException eFailCrtSvr(ERR_NULLHANDLE, "Failed to create uninstall mcf service!\n");

	bool isInstaled = getItemInfo()->isInstalled();
	bool isInstalling = HasAllFlags(getItemInfo()->getStatus(), UserCore::Item::ItemInfoI::STATUS_INSTALLING);

	getUserCore()->getItemManager()->killAllProcesses(getItemId());

	if (!UIBaseServiceTask::initService())
	{
		onComplete();
		return false;
	}

	if (!isInstaled && !isInstalling)
	{
		onComplete();
		return false;
	}

	gcString insPath = getItemInfo()->getPath();
	gcString mcfPath = getBranchMcf(getItemInfo()->getId(), getItemInfo()->getInstalledBranch(), getItemInfo()->getInstalledBuild());
	m_pIPCUI = getServiceMain()->newUninstallMcf();

	if (!m_pIPCUI)
	{
		onErrorEvent(eFailCrtSvr);
		return false;
	}

	m_pIPCUI->onCompleteEvent += delegate(this, &UIServiceTask::onComplete);
	m_pIPCUI->onProgressEvent += delegate(&onMcfProgressEvent);
	m_pIPCUI->onErrorEvent += delegate(this, &UIServiceTask::onServiceError);

	m_bRunning = true;
	m_pIPCUI->start(mcfPath.c_str(), insPath.c_str(), getItemInfo()->getInstallScriptPath());

	return true;
}


void UIServiceTask::onServiceError(gcException& e)
{
	onErrorEvent(e);
	UIBaseServiceTask::onServiceError(e);
}

void UIServiceTask::onComplete()
{
	completeUninstall(m_bRemoveAll, m_bRemoveAcc);

	getItemHandle()->getInternal()->completeStage(true);

	UIBaseServiceTask::onComplete();
}
