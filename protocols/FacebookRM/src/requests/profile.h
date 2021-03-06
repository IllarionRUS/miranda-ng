/*

Facebook plugin for Miranda Instant Messenger
_____________________________________________

Copyright © 2011-17 Robert Pösel, 2017-18 Miranda NG team

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef _FACEBOOK_REQUEST_PROFILE_H_
#define _FACEBOOK_REQUEST_PROFILE_H_

// getting own name, avatar, ...
class HomeRequest : public HttpRequest
{
public:
	HomeRequest() :
		HttpRequest(REQUEST_GET, FACEBOOK_SERVER_MOBILE "/profile.php")
	{
		flags |= NLHRF_REDIRECT;

		Url << CHAR_PARAM("v", "info");
	}
};

// getting fb_dtsg
class DtsgRequest : public HttpRequest
{
public:
	DtsgRequest() :
		HttpRequest(REQUEST_GET, FACEBOOK_SERVER_MOBILE "/profile/basic/intro/bio/")
	{
		flags |= NLHRF_REDIRECT;
	}
};

// request mobile page containing profile picture
class ProfilePictureRequest : public HttpRequest
{
public:
	ProfilePictureRequest(bool mobileBasicWorks, const char *userId) :
		HttpRequest(REQUEST_GET, FORMAT, "%s/profile/picture/view/", mobileBasicWorks ? FACEBOOK_SERVER_MBASIC : FACEBOOK_SERVER_MOBILE)
	{
		flags |= NLHRF_REDIRECT;

		Url << CHAR_PARAM("profile_id", userId);
	}
};

// request mobile page containing user profile
class ProfileRequest : public HttpRequest
{
public:
	ProfileRequest(bool mobileBasicWorks, const char *data) :
		HttpRequest(REQUEST_GET, FORMAT, "%s/%s", mobileBasicWorks ? FACEBOOK_SERVER_MBASIC : FACEBOOK_SERVER_MOBILE, data)
	{
		Url << CHAR_PARAM("v", "info");
	}
};

// request mobile page containing user profile by his id, and in english language (for parsing data)
class ProfileInfoRequest : public HttpRequest
{
public:
	ProfileInfoRequest(bool mobileBasicWorks, const char *userId) :
		HttpRequest(REQUEST_GET, FORMAT, "%s/profile.php", mobileBasicWorks ? FACEBOOK_SERVER_MBASIC : FACEBOOK_SERVER_MOBILE)
	{
		Url
			<< CHAR_PARAM("id", userId)
			<< CHAR_PARAM("v", "info")
			<< CHAR_PARAM("locale", "en_US");
	}
};

#endif //_FACEBOOK_REQUEST_PROFILE_H_
