/*
Desura is the leading indie game distribution platform
Copyright (C) 2011 Mark Chandler (Desura Net Pty Ltd)

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

#ifndef DESURA_GUIDELEGATE_H
#define DESURA_GUIDELEGATE_H
#ifdef _WIN32
#pragma once
#endif

#include <wx/wx.h>
#include "Event.h"
#include "util_thread/BaseThread.h"

#include <type_traits>
#include <memory>
#include <atomic>

class gcPanel;
class gcDialog;
class gcScrolledWindow;
class gcFrame;
class gcTaskBarIcon;


enum MODE
{
	MODE_PENDING,
	MODE_PENDING_WAIT,
	MODE_PROCESS,
};

uint64 GetMainThreadId();


class EventHelper
{
public:
	EventHelper()
	{
		m_bDone = false;
	}

	void done()
	{
		if (m_bDone)
			return;

		m_bDone = true;
		m_WaitCond.notify();
	}

	void wait()
	{
		while (!m_bDone)
			m_WaitCond.wait(0, 500);
	}
	
	bool isDone() const
	{
		return m_bDone;
	}

private:
	Thread::WaitCondition m_WaitCond;
	volatile bool m_bDone;
};

class Invoker
{
public:
	Invoker(std::function<void()> &fnCallback)
		: m_fnCallback(fnCallback)
	{
	}

	~Invoker()
	{
		cancel();
	}

	void invoke()
	{
		std::lock_guard<std::mutex> guard(m_Lock);
		m_fnCallback();
		m_pHelper.done();
	}

	void cancel()
	{
		std::lock_guard<std::mutex> guard(m_Lock);
		m_fnCallback = std::function<void()>();
		m_pHelper.done();
	}

	void wait()
	{
		m_pHelper.wait();
	}

	bool isCanceled()
	{
		std::lock_guard<std::mutex> guard(m_Lock);
		return m_pHelper.isDone();
	}

private:
	std::mutex m_Lock;
	EventHelper m_pHelper;
	std::function<void()> m_fnCallback;
};

class wxGuiDelegateEvent : public wxNotifyEvent
{
public:
	wxGuiDelegateEvent();
	wxGuiDelegateEvent(std::shared_ptr<Invoker> &invoker, int winId);
	wxGuiDelegateEvent(const wxGuiDelegateEvent& event);

	~wxGuiDelegateEvent();

	wxEvent *Clone() const override;
	void invoke();

private:
	std::shared_ptr<Invoker> m_pDelegate;
	DECLARE_DYNAMIC_CLASS(wxGuiDelegateEvent);
};

wxDECLARE_EVENT(wxEVT_GUIDELEGATE, wxGuiDelegateEvent);

template <typename T>
class wxGuiDelegateImplementation : public T
{
public:
	wxGuiDelegateImplementation(wxWindow *parent)
		: T(parent)
	{
		this->Bind(wxEVT_GUIDELEGATE, &wxGuiDelegateImplementation::onEventCallBack, this);
	}

	wxGuiDelegateImplementation(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
		: T(parent, id, pos, size, style)
	{
		this->Bind(wxEVT_GUIDELEGATE, &wxGuiDelegateImplementation::onEventCallBack, this);
	}

	wxGuiDelegateImplementation(wxWindow *parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style)
		: T(parent, id, title, pos, size, style)
	{
		this->Bind(wxEVT_GUIDELEGATE, &wxGuiDelegateImplementation::onEventCallBack, this);
	}

	~wxGuiDelegateImplementation()
	{
		this->Unbind(wxEVT_GUIDELEGATE, &wxGuiDelegateImplementation::onEventCallBack, this);
		cleanUpEvents();
	}

	void registerDelegate(InvokeI* d)
	{
		doDeregisterDelegate(d);

		std::lock_guard<std::mutex> guard(m_ListLock);
		m_vDelgateList.push_back(d);
	}

	void deregisterDelegate(InvokeI* d)
	{
		bool bFound = doDeregisterDelegate(d);
		assert( bFound );
	}

	void cleanUpEvents()
	{
		std::lock_guard<std::mutex> guard(m_ListLock);

		for (auto d : m_vDelgateList)
			d->cancel();

		m_vDelgateList.clear();
	}

private:
	bool doDeregisterDelegate(InvokeI* d)
	{
		std::lock_guard<std::mutex> guard(m_ListLock);

		auto it = std::remove(begin(m_vDelgateList), end(m_vDelgateList), d);

		if (it == end(m_vDelgateList))
			return false;

		m_vDelgateList.erase(it, end(m_vDelgateList));
		return true;
	}

	void onEventCallBack(wxGuiDelegateEvent& event)
	{
		event.invoke();
	}

	std::mutex m_ListLock;
	std::vector<InvokeI*> m_vDelgateList;
};


class RefParamsI
{
public:
	virtual ~RefParamsI(){}
};

template <typename T>
class RefParam : public RefParamsI
{
public:
	typedef typename std::remove_const<typename std::remove_reference<T>::type>::type NonConstType;

	RefParam(T t)
		: m_T(t)
	{
	}

	virtual ~RefParam(){}

	NonConstType m_T;
};

template <typename TObj, typename ... Args>
class GuiDelegate : public DelegateBase<Args...>, public InvokeI
{
public:
	GuiDelegate(std::function<void(Args&...)> callback, uint64 compareHash, TObj *pObj, MODE mode)
		: DelegateBase<Args...>(callback, compareHash)
		, m_Mode(mode)
		, m_pObj(pObj)
	{
		assert(m_pObj);

		if (m_pObj)
			m_pObj->registerDelegate(this);
	}

	~GuiDelegate()
	{
		cancel();
	}

	void destroy() override
	{
		delete this;
	}

	void cancel() override
	{
		std::lock_guard<std::mutex> guard(m_InvokerMutex);

		m_bCanceled = true;

		if (m_pInvoker)
			m_pInvoker->cancel();

		if (m_pObj)
			m_pObj->deregisterDelegate(this);

		m_pObj = nullptr;
	}

	DelegateI<Args...>* clone() override
	{
		return new GuiDelegate(DelegateBase<Args...>::m_fnCallback, DelegateBase<Args...>::getCompareHash(), m_pObj, m_Mode);
	}

	void callback(Args& ... args)
	{
		if (m_bCanceled)
			return;

		DelegateBase<Args...>::operator()(args...);
	}

	void operator()(Args&... args) override
	{	
		if (m_bCanceled)
			return;

		if (m_Mode == MODE_PENDING)
		{
			std::function<void()> pcb = std::bind(&GuiDelegate<TObj, Args...>::callback, this, args...);

			auto invoker = std::make_shared<Invoker>(pcb);
			auto event = new wxGuiDelegateEvent(invoker, m_pObj->GetId());
			m_pObj->GetEventHandler()->QueueEvent(event);

		}
		else if (m_Mode == MODE_PROCESS || Thread::BaseThread::GetCurrentThreadId() == GetMainThreadId())
		{
			DelegateBase<Args...>::operator()(args...);
		}
		else if (m_Mode == MODE_PENDING_WAIT)
		{
			std::function<void()> pcb = std::bind(&GuiDelegate<TObj, Args...>::callback, this, std::ref(args)...);

			auto invoker = std::make_shared<Invoker>(pcb);
			auto event = new wxGuiDelegateEvent(invoker, m_pObj->GetId());
			m_pObj->GetEventHandler()->QueueEvent(event);

			setInvoker(invoker);
			invoker->wait();
			setInvoker(std::shared_ptr<Invoker>());
		}
	}

protected:
	void setInvoker(const std::shared_ptr<Invoker> &i)
	{
		std::lock_guard<std::mutex> guard(m_InvokerMutex);
		m_pInvoker = i;
	}

private:
	MODE m_Mode;
	TObj *m_pObj = nullptr;
	std::atomic<bool> m_bCanceled;
	std::mutex m_InvokerMutex;
	std::shared_ptr<Invoker> m_pInvoker;
};


template <typename TObj>
inline bool validateForm(TObj* pObj)
{
	gcPanel* pan = dynamic_cast<gcPanel*>(pObj);
	gcFrame* frm = dynamic_cast<gcFrame*>(pObj);
	gcDialog* dlg = dynamic_cast<gcDialog*>(pObj);
	gcScrolledWindow* swin = dynamic_cast<gcScrolledWindow*>(pObj);
	gcTaskBarIcon* gtbi = dynamic_cast<gcTaskBarIcon*>(pObj);

	return (pan || frm || dlg || swin || gtbi);
}

template <typename TObj, typename ... Args>
DelegateI<Args...>* guiDelegate(TObj* pObj, void (TObj::*fnCallback)(Args...), MODE mode = MODE_PENDING)
{
	if (!validateForm(pObj))
	{
		assert(false);
		return nullptr;
	}

	std::function<void(Args...)> callback = [pObj, fnCallback](Args...args)
	{
		(*pObj.*fnCallback)(args...);
	};

	return new GuiDelegate<TObj, Args...>(callback, MakeUint64(pObj, (void*)&typeid(fnCallback)), pObj, mode);
}

template <typename TObj, typename ... Args, typename TExtra>
DelegateI<Args...>* guiExtraDelegate(TObj* pObj, void (TObj::*fnCallback)(TExtra, Args...), TExtra tExtra, MODE mode = MODE_PENDING)
{
	if (!validateForm(pObj))
	{
		assert(false);
		return nullptr;
	}

	std::function<void(Args...)> callback = [pObj, fnCallback, tExtra](Args...args)
	{
		(*pObj.*fnCallback)(tExtra, args...);
	};

	return new GuiDelegate<TObj, Args...>(callback, MakeUint64(pObj, (void*)&typeid(fnCallback)), pObj, mode);
}

#endif //DESURA_GUIDELEGATE_H
