/*
Copyright (c) 2015-18 Miranda NG team (https://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SKYPE_REQUEST_QUEUE_H_
#define _SKYPE_REQUEST_QUEUE_H_

typedef void(*HttpResponseCallback)(const NETLIBHTTPREQUEST *response, void *arg);

struct RequestQueueItem
{
	void *arg;
	HttpRequest *request;
	HttpResponseCallback responseCallback;

	RequestQueueItem(HttpRequest *request, void *arg) :
		 arg(arg), request(request), responseCallback(NULL) { }

	RequestQueueItem(HttpRequest *request, HttpResponseCallback response, void *arg) :
		 arg(arg), request(request), responseCallback(response) { }

	~RequestQueueItem()
	{
		delete request;
		request = NULL;
		responseCallback = NULL;
	}
};

class RequestQueue
{
	bool isTerminated;
	HNETLIBUSER nlu;
	mir_cs requestQueueLock;
	LIST<RequestQueueItem> requests;
	EventHandle hRequestQueueEvent;
	HANDLE hRequestQueueThread;

	void Execute(RequestQueueItem *item);

	static unsigned int __cdecl AsyncSendThread(void*, void*);
	static unsigned int __cdecl WorkerThread(void*);

public:
	RequestQueue(HNETLIBUSER nlu);
	~RequestQueue();

	void Start();
	void Stop();

	void Push(HttpRequest *request, HttpResponseCallback response = NULL, void *arg = NULL);
	void Send(HttpRequest *request, HttpResponseCallback response = NULL, void *arg = NULL);

};

#endif //_SKYPE_REQUEST_QUEUE_H_