/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef DAEMON_CORE_SOCK_ADAPTER_H
#define DAEMON_CORE_SOCK_ADAPTER_H

/**
   This class is used as an indirect way to call daemonCore functions
   from cedar sock code.  Not all applications that use cedar
   are linked with DaemonCore (or use the DaemonCore event loop).
   In such applications, this daemonCore interface class will not
   be initialized and it is an error if these functions are ever
   used in such cases.
 */

#include "../condor_daemon_core.V6/condor_daemon_core.h"

class DaemonCoreSockAdapterClass {
 public:
	typedef int (DaemonCore::*Register_Socket_fnptr)(Stream*,const char*,SocketHandlercpp,const char*,Service*,DCpermission);
	typedef int (DaemonCore::*Cancel_Socket_fnptr)( Stream *sock );
    typedef int (DaemonCore::*Register_DataPtr_fnptr)( void *data );
    typedef void *(DaemonCore::*GetDataPtr_fnptr)();
	typedef int (DaemonCore::*Register_Timer_fnptr)(unsigned deltawhen,Eventcpp event,char * event_descrip,Service* s);
	typedef bool (DaemonCore::*TooManyRegisteredSockets_fnptr)(int fd,MyString *msg);


	DaemonCoreSockAdapterClass(): m_daemonCore(0) {}

	void EnableDaemonCore(
		DaemonCore *daemonCore,
		Register_Socket_fnptr Register_Socket_fptr,
		Cancel_Socket_fnptr Cancel_Socket_fptr,
		Register_DataPtr_fnptr Register_DataPtr_fptr,
		GetDataPtr_fnptr GetDataPtrFun_fptr,
		Register_Timer_fnptr Register_Timer_fptr,
		TooManyRegisteredSockets_fnptr TooManyRegisteredSockets_fptr)
	{
		m_daemonCore = daemonCore;
		m_Register_Socket_fnptr = Register_Socket_fptr;
		m_Cancel_Socket_fnptr = Cancel_Socket_fptr;
		m_Register_DataPtr_fnptr = Register_DataPtr_fptr;
		m_GetDataPtr_fnptr = GetDataPtrFun_fptr;
		m_Register_Timer_fnptr = Register_Timer_fptr;
		m_TooManyRegisteredSockets_fnptr = TooManyRegisteredSockets_fptr;
	}

		// These functions all have the same interface as the corresponding
		// daemonCore functions.

	DaemonCore *m_daemonCore;
	Register_Socket_fnptr m_Register_Socket_fnptr;
	Cancel_Socket_fnptr m_Cancel_Socket_fnptr;
	Register_DataPtr_fnptr m_Register_DataPtr_fnptr;
	GetDataPtr_fnptr m_GetDataPtr_fnptr;
	Register_Timer_fnptr m_Register_Timer_fnptr;
	TooManyRegisteredSockets_fnptr m_TooManyRegisteredSockets_fnptr;

    int Register_Socket (Stream*              iosock,
                         const char *         iosock_descrip,
                         SocketHandlercpp     handlercpp,
                         const char *         handler_descrip,
                         Service*             s,
                         DCpermission         perm = ALLOW)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Socket_fnptr)(iosock,iosock_descrip,handlercpp,handler_descrip,s,perm);
	}

	int Cancel_Socket( Stream *stream )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Cancel_Socket_fnptr)(stream);
	}

    int Register_DataPtr( void *data )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_DataPtr_fnptr)(data);
	}
    void *GetDataPtr()
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_GetDataPtr_fnptr)();
	}
    int Register_Timer (unsigned     deltawhen,
                        Eventcpp        event,
                        char *       event_descrip, 
                        Service*     s = NULL)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Timer_fnptr)(
			deltawhen,
			event,
			event_descrip,
			s);
	}
	bool TooManyRegisteredSockets(int fd=-1,MyString *msg=NULL)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_TooManyRegisteredSockets_fnptr)(fd,msg);
	}

	bool isEnabled()
	{
		return m_daemonCore != NULL;
	}
};

extern DaemonCoreSockAdapterClass daemonCoreSockAdapter;

#endif
