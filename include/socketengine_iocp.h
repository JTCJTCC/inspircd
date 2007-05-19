/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __SOCKETENGINE_IOCP__
#define __SOCKETENGINE_IOCP__

#define READ_BUFFER_SIZE 500
#define USING_IOCP 1

#include "inspircd_config.h"
#include "inspircd_win32wrapper.h"
#include "globals.h"
#include "inspircd.h"
#include "socketengine.h"

enum SocketIOEvent
{
	SOCKET_IO_EVENT_READ_READY			= 0,
	SOCKET_IO_EVENT_WRITE_READY			= 1,
	SOCKET_IO_EVENT_ACCEPT				= 2,
	SOCKET_IO_EVENT_ERROR				= 3,
	NUM_SOCKET_IO_EVENTS				= 4,
};

class Overlapped
{
public:
	OVERLAPPED m_overlap;
	SocketIOEvent m_event;
	int m_params;
	Overlapped(SocketIOEvent ev, int params) : m_event(ev), m_params(params)
	{
		memset(&m_overlap, 0, sizeof(OVERLAPPED));
	}
};

struct accept_overlap
{
	int socket;
	char buf[1024];
};

class IOCPEngine : public SocketEngine
{
	/** Creates a "fake" file descriptor for use with an IOCP socket.
	 * @return -1 if there are no free slots, and an integer if it finds one.
	 */
	__inline int GenerateFd()
	{
		register int i = 0;
		for(; i < MAX_DESCRIPTORS; ++i)
			if(ref[i] == 0)
				return i;
		return -1;
	}
	
	/** Global I/O completion port that sockets attach to.
	 */
	HANDLE m_completionPort;

	/** This is kinda shitty... :/ for getting an address from a real fd. 
	 */
	map<int, EventHandler*> m_binding;

public:
	/** Creates an IOCP Socket Engine
	 * @param Instance The creator of this object
	 */
	IOCPEngine(InspIRCd * Instance);

	/** Deletes an IOCP socket engine and all the attached sockets
	 */
	~IOCPEngine();

	/** Adds an event handler to the completion port, and sets up initial events.
	 * @param eh EventHandler to add
	 * @return True if success, false if no room
	 */
	bool AddFd(EventHandler* eh);

	/** Gets the maximum number of file descriptors that this engine can handle.
	 * @return The number of file descriptors
	 */
	__inline int GetMaxFds() { return MAX_DESCRIPTORS; }

	/** Gets the number of free/remaining file descriptors under this engine.
	 * @return Remaining count
	 */
	__inline int GetRemainingFds()
	{
		register int count = 0;
		register int i = 0;			
		for(; i < MAX_DESCRIPTORS; ++i)
			if(ref[i] == 0)
				++count;
		return count;
	}

	/** Removes a file descriptor from the set, preventing it from receiving any more events
	 * @return True if remove was successful, false otherwise
	 */
	bool DelFd(EventHandler* eh, bool force = false);

	/** Called every loop to handle input/output events for all sockets under this engine
	 * @return The number of "changed" sockets.
	 */
	int DispatchEvents();

	/** Gets the name of this socket engine as a string.
	 * @return string of socket engine name
	 */
	std::string GetName();

	/** Queues a Write event on the specified event handler.
	 * @param eh EventHandler that needs data sent on
	 */
	void WantWrite(EventHandler* eh);

	/** Posts a completion event on the specified socket.
	 * @param eh EventHandler for message
	 * @param type Event Type
	 * @param param Event Parameter
	 * @return True if added, false if not
	 */
	bool PostCompletionEvent(EventHandler * eh, SocketIOEvent type, int param);

	/** Posts a read event on the specified socket
	 * @param eh EventHandler (socket)
	 */
	void PostReadEvent(EventHandler * eh);

	/** Posts an accept event on the specified socket
	 * @param eh EventHandler (socket)
	 */
	void PostAcceptEvent(EventHandler * eh);

	/** Returns the EventHandler attached to a specific fd.
	 * If the fd isnt in the socketengine, returns NULL.
	 * @param fd The event handler to look for
	 * @return A pointer to the event handler, or NULL
	 */
	EventHandler* GetRef(int fd);

	/** Returns true if a file descriptor exists in
	 * the socket engine's list.
	 * @param fd The event handler to look for
	 * @return True if this fd has an event handler
	 */
	bool HasFd(int fd);

	/** Returns the EventHandler attached to a specific fd.
	 * If the fd isnt in the socketengine, returns NULL.
	 * @param fd The event handler to look for
	 * @return A pointer to the event handler, or NULL
	 */
	EventHandler* GetIntRef(int fd);
};

//typedef void(*OpHandler)(EventHandler)
/** Event Handler Array
 */

/** Creates a SocketEngine
 */
class SocketEngineFactory
{
public:
	/** Create a new instance of SocketEngine based on IOCPEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new IOCPEngine(Instance); }
};

#endif
