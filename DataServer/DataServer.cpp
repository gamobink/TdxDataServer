// DataServer.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "../Plugin/Plugin.h"
#include <cstdarg>
#include <stdio.h>
#include <WinSock2.h>
#include <fstream>
#include <atlstr.h>
#include <atlcoll.h>

#pragma comment(lib, "ws2_32")

using namespace std;

void DebugInfo(const char* format, ...)
{
#pragma warning(disable : 4996)
    char line[1024]={0};
    va_list ap;	
    va_start(ap, format);
    vsprintf(line, format, ap);
    va_end(ap);
    OutputDebugString(line);
#pragma warning(default : 4996)
}

#define FDS_SIZE (10)
typedef int FDS[FDS_SIZE];
void fds_copy(FD_SET *set, const FDS fds) {
    FD_ZERO(set);
    for (int i = 0; i < FDS_SIZE; i++) {
        if (fds[i] != INVALID_SOCKET)
            FD_SET(fds[i], set);
    }
}

void fds_clear(FDS fds) {
    for (int i = 0; i < FDS_SIZE; i++) {
        fds[i] = INVALID_SOCKET;
    }
}

int fds_add(FDS fds, int fd) {
    for (int i = 0; i < FDS_SIZE; i++) {
        if (fds[i] == INVALID_SOCKET) {
            fds[i] = fd;
            return i;
        }
    }
    return -1;
}

void fds_del(FDS fds, int fd) {
    for (int i = 0; i < FDS_SIZE; i++) {
        if (fds[i] == fd) {
            fds[i] = INVALID_SOCKET;
            return;
        }
    }
}

struct Buff {
    char *beg;
    char *r;
    char *w;
    char *end;
};

void buff_clear(struct Buff *buff) {
    buff->r = buff->w = buff->beg;
    *(buff->beg) = '\0';
}

bool buff_alloc(struct Buff *buff, int cap) {
    memset(buff, 0, sizeof(struct Buff));
    if (buff->beg = (char *)malloc(cap)) {
        buff->end = buff->beg+cap-1;
        *(buff->end) = '\0';
        buff_clear(buff);
        return true;
    }
    return false;
}

void buff_free(struct Buff *buff) {
    if (buff->beg) {
        free(buff->beg);
        memset(buff, 0, sizeof(struct Buff));
    }
}

char* buff_r(struct Buff *buff) {
    return buff->r;
}

char* buff_w(struct Buff *buff) {
    return buff->w;
}

void buff_inc_r(struct Buff *buff, int count) {
    buff->r += count;
}

void buff_inc_w(struct Buff *buff, int count) {
    buff->w += count;
    *(buff->w) = '\0';
}

int buff_can_r(struct Buff *buff) {
    return buff->w - buff->r;
}

int buff_can_w(struct Buff *buff) {
    return buff->end - buff->w;
}

static int Send(SOCKET s, const char* buf, int len) {
    int done = 0;
    while (done < len) {
        FD_SET fdset;
        FD_ZERO(&fdset);
        FD_SET(s, &fdset);
        struct timeval timeout = {0};
        timeout.tv_sec = 3;
        if (select(s+1, NULL, &fdset, NULL, &timeout) > 0) {
            if (FD_ISSET(s, &fdset)) {
                int n = send(s, buf+done, len-done, 0);
                if (n >= 0) {
                    done += n;
                } else {
                    break;
                }
            }
        } else {
            DebugInfo("busy waiting for send\n");
        }
    }
    return done;
}

static int Recv(SOCKET s, char* buf, int len) {
    int done = 0;
    while (done < len) {
        FD_SET fdset;
        FD_ZERO(&fdset);
        FD_SET(s, &fdset);
        struct timeval timeout = {0};
        timeout.tv_sec = 3;
        if (select(s+1, &fdset, NULL, NULL, &timeout) > 0) {
            if (FD_ISSET(s, &fdset)) {
                int n = recv(s, buf+done, len-done, 0);
                if (n > 0) {
                    done += n;
                } else {
                    break;
                }
            }
        } else {
            DebugInfo("busy waiting for recv\n");
        }
    }
}

const BYTE INVALID_DATA[]={0xF8,0xF8,0xF8,0xF8};
const NTime INVALID_TIME={0};

typedef CAtlMap<CString, CString> StringMap;
typedef CAtlArray<CString> StringArray;
void parse_request(const CString req, StringMap& ret) {
    int pos = 0;
    ret.RemoveAll();
    while (pos != -1) {
        CString tok = req.Tokenize("\r\n", pos);
        if (!tok.IsEmpty()) {
            int colon = tok.Find(':');
            if (colon != -1) {
                CString name = tok.Mid(0, colon).Trim();
                CString value = tok.Mid(colon+1).Trim();
                if (!name.IsEmpty() && !value.IsEmpty()) {
                    ret[name] = value;
                }
            }
        }
    }
}

void print_map(const StringMap& m) {
    POSITION pos = m.GetStartPosition();
    while (pos != NULL) {
        CString k, v;
        m.GetNextAssoc(pos, k, v);
        DebugInfo("%s -> %s\n", k.GetString(), v.GetString());
    }
}

void print_time(CString what, NTime tm) {
    DebugInfo("%s %d-%d-%d %d:%d:%d\n", what.GetString(), tm.year, tm.month, tm.day, 
        tm.hour, tm.minute, tm.second);
}

void split(StringArray& strs, CString str, char by) {
    strs.RemoveAll();
    while (!str.IsEmpty()) {
        int i = str.Find(by);
        if (i == -1) {
            strs.Add(str);
            break;
        }
        if (i != 0)
            strs.Add(str.Mid(0, i));
        str = str.Mid(i+1);
    }
}

NTime parse_time(CString str) {
    NTime tm = INVALID_TIME;
    StringArray strs;
    split(strs, str, ' ');
    if (strs.GetCount() != 2)
        return tm;
    CString date = strs[0];
    CString time = strs[1];
    split(strs, date, '-');
    if (strs.GetCount() != 3)
        return tm;
    tm.year = atoi(strs[0].GetString());
    tm.month = atoi(strs[1].GetString());
    tm.day = atoi(strs[2].GetString());
    split(strs, time, ':');
    if (strs.GetCount() != 3)
        return INVALID_TIME;
    tm.hour = atoi(strs[0].GetString());
    tm.minute = atoi(strs[1].GetString());
    tm.second = atoi(strs[2].GetString());
    return tm;
}

PDATAIOFUNC	 g_query = NULL;

const char* CODE = "Code";
const char* MARKET = "Market";
const char* TYPE = "Type";
const char* FROM = "From"; // Time From
const char* TO = "To";

void response(const StringMap& req, struct Buff *buff) {
    buff_clear(buff);
    if (req.Lookup(CODE) == NULL ||
        req.Lookup(MARKET) == NULL ||
        req.Lookup(TYPE) == NULL) {
        DebugInfo("invalid request\n");
        return;
    }
    const StringMap::CPair *p = NULL;
    p = req.Lookup(CODE);
    CString code = p->m_value;
    p = req.Lookup(MARKET);
    short market = atoi(p->m_value.GetString());
    p = req.Lookup(TYPE);
    short type = atoi(p->m_value.GetString());
    DebugInfo("query %s, %s, %d\n", code.GetString(), market==0 ? "Shenzhen" : "Shanghai", type);

    NTime from = INVALID_TIME, to = INVALID_TIME;
    p = req.Lookup(FROM);
    if (p) {
        from = parse_time(p->m_value);
    }
    print_time(FROM, from);
    p = req.Lookup(TO);
    if (p) {
        to = parse_time(p->m_value);
    }
    print_time(TO, to);

    if (g_query) {
        if (type >= PER_MIN5 && type <= PER_PRD_DIY10) {
            long data_num = g_query(
                (char*)code.GetString(),
                market,
                type,
                NULL,
                ASK_ALL,
                from,
                to,
                1, 0);
            LPHISDAT hist = new HISDAT[data_num];
            data_num = g_query(
                (char*)code.GetString(),
                market,
                type,
                hist,
                data_num,
                from,
                to,
                1, 0);
            for (long i = 0; i < data_num; i++) {
                char tmp[512];
                int data_length = _snprintf(tmp, sizeof(tmp), "Time:%d-%d-%d %d:%d:%d, Open:%f, High:%f, Low:%f, Close:%f, Amount:%f, Volume:%f, Up:%d, Down:%d\n", 
                    hist[i].Time.year, hist[i].Time.month, hist[i].Time.day,
                    hist[i].Time.hour, hist[i].Time.minute, hist[i].Time.second,
                    hist[i].Open, hist[i].High, hist[i].Low, hist[i].Close,
                    hist[i].Amount, hist[i].fVolume, hist[i].zd.up, hist[i].zd.down);
                if (data_length >= sizeof(tmp)) {
                    DebugInfo("data length exceed.\n");
                    break;
                }
                if (data_length > buff_can_w(buff)) {
                    DebugInfo("buff not enough.\n");
                    break;
                }
                memcpy(buff_w(buff), tmp, data_length);
                buff_inc_w(buff, data_length);
            }
            delete [] hist;
        } else if (type == REPORT_DAT2) {
            DebugInfo("REPORT_DAT2 not supported yet.\n");
        } else if (type == GBINFO_DAT) {
            GBINFO info = {0};
            long data_num = g_query(
                (char*)code.GetString(),
                market,
                type,
                &info,
                1,
                from,
                to,
                1, 0);
            char tmp[512];
            int data_length = _snprintf(tmp, sizeof(tmp), "Zgb:%f, Ltgb:%f\n", info.Zgb, info.Ltgb);
            if (data_length >= sizeof(tmp)) {
                DebugInfo("data length exceed.\n");
                return;
            }
            if (data_length > buff_can_w(buff)) {
                DebugInfo("buff not enough.\n");
                return;
            }
            memcpy(buff_w(buff), tmp, data_length);
            buff_inc_w(buff, data_length);
        } else if (type == STKINFO_DAT) {
            STOCKINFO info = {0};
            long data_num = g_query(
                (char*)code.GetString(),
                market,
                type,
                &info,
                1,
                from,
                to,
                1, 0);
            char tmp[512];
            int data_length = _snprintf(tmp, sizeof(tmp), "Name:%s\n", info.Name);
            if (data_length >= sizeof(tmp)) {
                DebugInfo("data length exceed.\n");
                return;
            }
            if (data_length > buff_can_w(buff)) {
                DebugInfo("buff not enough.\n");
                return;
            }
            memcpy(buff_w(buff), tmp, data_length);
            buff_inc_w(buff, data_length);
        } else if (type == TPPRICE_DAT) {
            TPPRICE info = {0};
            long data_num = g_query(
                (char*)code.GetString(),
                market,
                type,
                &info,
                1,
                from,
                to,
                1, 0);
            char tmp[512];
            int data_length = _snprintf(tmp, sizeof(tmp), "Close:%f, TPTop:%f, TPBottom:%f\n", info.Close, info.TPTop, info.TPBottom);
            if (data_length >= sizeof(tmp)) {
                DebugInfo("data length exceed.\n");
                return;
            }
            if (data_length > buff_can_w(buff)) {
                DebugInfo("buff not enough.\n");
                return;
            }
            memcpy(buff_w(buff), tmp, data_length);
            buff_inc_w(buff, data_length);
        } else {
            DebugInfo("invalid query type: %d\n", type);
        }
    } else {
        int data_length = sprintf(buff_w(buff), "Code:%s, Market:%d, Type:%d, From:%d-%d-%d %d:%d:%d, To:%d-%d-%d %d:%d:%d\n", 
            code.GetString(), market, type, 
            from.year, from.month, from.day, from.hour, from.minute, from.second, 
            to.year, to.month, to.day, to.hour, to.minute, to.second);
        buff_inc_w(buff, data_length);
    }
}

int start_server(unsigned short *pport) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
        DebugInfo("create socket failed\n");
        return INVALID_SOCKET;
    }

    struct sockaddr_in addr = {0};
    int len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0;
    if (bind(fd, (sockaddr *)&addr, len)) {
        DebugInfo("bind socket failed\n");
        closesocket(fd);
        return INVALID_SOCKET;
    }

    if (getsockname(fd, (sockaddr *)&addr, &len)) {
        DebugInfo("getsockname failed\n");
        closesocket(fd);
        return INVALID_SOCKET;
    }
    if (listen(fd, 10)) {
        DebugInfo("listen failed\n");
        closesocket(fd);
        return INVALID_SOCKET;
    }

    *pport = ntohs(addr.sin_port);
    return fd;
}

void save_server_port(unsigned short port) {
    ofstream ofs("C:/zd_cczq/DataServer/server-port.txt");
    ofs << port;
    DebugInfo("server at: %d\n", port);
}

// accept -> put into read q -> clear rbuff -> read all -> shutdown receive ->
// remove from read q -> put into write q -> clear wbuff -> prepare response -> 
// write all -> close socket -> remove from write q
void server_loop(int svr_fd) {
    bool dosth = false;
    FDS rfds, wfds;
    fds_clear(rfds);
    fds_clear(wfds);
    int rfds_cap = FDS_SIZE;
    FD_SET rfdset, wfdset;
    struct Buff rbuffs[FDS_SIZE], wbuffs[FDS_SIZE];
    for (int i = 0; i < FDS_SIZE; i++) {
        buff_alloc(rbuffs+i, 512);
        buff_alloc(wbuffs+i, 4096);
    }
    while (true) {
        dosth = false;
        fds_copy(&rfdset, rfds);
        FD_SET(svr_fd, &rfdset);
        fds_copy(&wfdset, wfds);
        if (select(0, &rfdset, &wfdset, NULL, NULL) > 0) {
            if (FD_ISSET(svr_fd, &rfdset) && rfds_cap) {
                struct sockaddr_in addr = {0};
                int len = sizeof(addr);
                int fd = accept(svr_fd, (sockaddr *)&addr, &len);
                if (fd != INVALID_SOCKET) {
                    DebugInfo("accept connection from: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                    int idx = fds_add(rfds, fd);
                    rfds_cap--;
                    buff_clear(rbuffs+idx);
                    dosth = true;
                }
            }

            for (int i= 0; i < FDS_SIZE; i++) {
                int fd = rfds[i];
                if (fd != INVALID_SOCKET && FD_ISSET(fd, &rfdset)) {
                    int len = recv(fd, buff_w(rbuffs+i), buff_can_w(rbuffs+i), 0);
                    DebugInfo("recv: %d bytes\n", len);
                    if (len > 0) {
                        buff_inc_w(rbuffs+i, len);
                    }
                    if (len <= 0 || buff_can_w(rbuffs+i) == 0) {
                        if (len < 0) {
                            DebugInfo("recv error: %d\n", WSAGetLastError());
                        }
                        if (buff_can_w(rbuffs+i) == 0) {
                            DebugInfo("recv buffer is full\n");
                        }
                        shutdown(fd, SD_RECEIVE);
                        fds_del(rfds, fd);

                        int idx = fds_add(wfds, fd);
                        StringMap fields;
                        parse_request(buff_r(rbuffs+i), fields);
                        response(fields, wbuffs+idx);
                        DebugInfo("%d bytes to be sent\n", buff_can_r(wbuffs+idx));
                    }
                    dosth = true;
                }
            }

            for (int i= 0; i < FDS_SIZE; i++) {
                int fd = wfds[i];
                if (fd != INVALID_SOCKET && FD_ISSET(fd, &wfdset)) {
                    int len = send(fd, buff_r(wbuffs+i), buff_can_r(wbuffs+i), 0);
                    if (len >= 0) {
                        buff_inc_r(wbuffs+i, len);
                    } else {
                        DebugInfo("send error: %d\n", WSAGetLastError());
                        buff_clear(wbuffs+i);
                    }

                    if (buff_can_r(wbuffs+i) == 0) {
                        DebugInfo("send done\n");
                        closesocket(fd);
                        fds_del(wfds, fd);
                        rfds_cap++;
                    }
                    dosth = true;
                }
            }
        }
        if (!dosth) {
            DebugInfo("something wrong...\n");
            Sleep(1000);
        }
    }
}

DWORD WINAPI ThreadFuncFirst(LPVOID param)
{
    unsigned short port = 0;
    int svr_fd = start_server(&port);
    if (svr_fd != INVALID_SOCKET) {
        save_server_port(port);
        server_loop(svr_fd);
        closesocket(svr_fd);
    }
    return 0;
}
/*
//TODO: response
NTime tmpTime={0};
short nDataNum = 5;
LPHISDAT pHisDat = new HISDAT[nDataNum];  //Êý¾Ý»º³åÇø
long readnum = g_query(buff_r(rbuffs+i),1,PER_DAY,pHisDat,nDataNum,tmpTime,tmpTime,1,0);
for(int i=0;i < readnum;i++)
{
    int x = sprintf(buff_w(wbuffs+idx), "close: %f\n", pHisDat[i].Close);
    buff_inc_w(wbuffs+idx, x);
}
delete []pHisDat;pHisDat=NULL;
*/
extern "C"  __declspec(dllexport)
void RegisterDataInterface(PDATAIOFUNC pfn) {
    if (g_query == NULL) {
        g_query = pfn;
        DWORD dwThreadID = 0;
        HANDLE handleFirst = CreateThread(NULL, 0, ThreadFuncFirst, 0, 0, &dwThreadID);
    }
}
