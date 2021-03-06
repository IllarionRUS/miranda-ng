#ifndef _STEAM_REQUEST_SESSION_H_
#define _STEAM_REQUEST_SESSION_H_

class GetSessionRequest : public HttpRequest
{
public:
	GetSessionRequest(const char *token, const char *steamId, const char *cookie) :
		HttpRequest(HttpPost, STEAM_WEB_URL "/mobileloginsucceeded")
	{
		flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMP;

		Content = new FormUrlEncodedContent(this)
			<< CHAR_PARAM("oauth_token", token)
			<< CHAR_PARAM("steamid", steamId)
			<< CHAR_PARAM("webcookie", cookie);

		char data[512];
		mir_snprintf(data, _countof(data),
			"oauth_token=%s&steamid=%s&webcookie=%s",
			token,
			steamId,
			cookie);
	}
};

class GetSessionRequest2 : public HttpRequest
{
public:
	GetSessionRequest2() :
		HttpRequest(HttpGet, STEAM_WEB_URL)
	{
		flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMP;
	}
};

#endif //_STEAM_REQUEST_SESSION_H_
