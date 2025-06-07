#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <thread>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
using namespace std;

struct User {
    string id;
    string password;
    bool isOnline = false;
};

class UserManager {
private:
    map<string, User> users;
    mutex mtx;

public:
    void load() {
        ifstream fin("users.txt");
        string id, pw;
        while (fin >> id >> pw) {
            users[id] = { id, pw, false };
        }
    }

    void save(const string& id, const string& pw) {
        ofstream fout("users.txt", ios::app);
        fout << id << " " << pw << "\n";
    }

    bool signup(const string& id, const string& pw) {
        lock_guard<mutex> lock(mtx);
        if (users.count(id)) return false;
        users[id] = { id, pw, false };
        save(id, pw);
        return true;
    }

    bool login(const string& id, const string& pw) {
        lock_guard<mutex> lock(mtx);
        return users.count(id) && users[id].password == pw;
    }

    void setOnline(const string& id, bool status) {
        lock_guard<mutex> lock(mtx);
        users[id].isOnline = status;
    }

    bool isOnline(const string& id) {
        lock_guard<mutex> lock(mtx);
        return users.count(id) && users[id].isOnline;
    }

    bool exists(const string& id) {
        lock_guard<mutex> lock(mtx);
        return users.count(id);
    }
};

class FriendManager {
private:
    map<string, vector<string>> friends;
    map<string, vector<string>> pending;
    mutex mtx;

public:
    void load() {
        ifstream ff("friends.txt");
        string line;
        while (getline(ff, line)) {
            istringstream iss(line);
            string id, f;
            iss >> id;
            while (iss >> f) friends[id].push_back(f);
        }

        ifstream pr("requests.txt");
        while (getline(pr, line)) {
            istringstream iss(line);
            string id, from;
            iss >> id;
            while (iss >> from) pending[id].push_back(from);
        }
    }

    void save() {
        ofstream ff("friends.txt");
        for (auto& p : friends) {
            ff << p.first;
            for (auto& f : p.second) ff << " " << f;
            ff << "\n";
        }

        ofstream pr("requests.txt");
        for (auto& p : pending) {
            pr << p.first;
            for (auto& r : p.second) pr << " " << r;
            pr << "\n";
        }
    }

    bool isFriend(const string& a, const string& b) {
        return find(friends[a].begin(), friends[a].end(), b) != friends[a].end();
    }

    void request(const string& from, const string& to) {
        pending[to].push_back(from);
        save();
    }

    bool hasRequest(const string& to, const string& from) {
        return find(pending[to].begin(), pending[to].end(), from) != pending[to].end();
    }

    void accept(const string& user, const string& from) {
        auto& p = pending[user];
        p.erase(remove(p.begin(), p.end(), from), p.end());
        friends[user].push_back(from);
        friends[from].push_back(user);
        save();
    }

    vector<string> getFriends(const string& id) {
        return friends[id];
    }

    vector<string> getRequests(const string& id) {
        return pending[id];
    }
};

class ChatServer {
private:
    SOCKET serverSock = INVALID_SOCKET;
    map<SOCKET, string> onlineUsers;
    set<SOCKET> chatParticipants;
    mutex onlineMutex, chatMutex;

    UserManager userManager;
    FriendManager friendManager;

    bool speedGameActive = false;
    string currentSpeedNumber;
    string speedWinner;
    chrono::steady_clock::time_point speedStartTime;

    bool updownActive = false;
    int updownAnswer = 0;
    map<string, int> updownTries;

    mutex gameMutex;

    string generateSpeedNumber() {
        string num = "";
        for (int i = 0; i < 4; ++i)
            num += '0' + rand() % 10;
        return num;
    }

    string getCurrentTimeString() {
        time_t now = time(nullptr);
        struct tm t;
        localtime_s(&t, &now);
        char buf[20];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
        return string(buf);
    }


    void sendMessage(SOCKET sock, const string& msg) {
        send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
    }

    void broadcast(const string& msg) {
        lock_guard<mutex> lock(chatMutex);
        for (SOCKET s : chatParticipants)
            sendMessage(s, msg);
    }

    void printOnlineUsers() {
        cout << ">> 현재 접속자: ";
        if (onlineUsers.empty()) {
            cout << "(없음)";
        }
        else {
            bool first = true;
            for (auto& p : onlineUsers) {
                if (!first) cout << ", ";
                cout << p.second;
                first = false;
            }
        }
        cout << "\n>> 현재 접속자 수: " << onlineUsers.size() << "\n";
        cout << "----------------------------------------------\n";
    }

    void handleClient(SOCKET sock) {
        char buffer[1024];
        string currentUser;

        while (true) {
            int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            string cmdline(buffer);

            istringstream iss(cmdline);
            string cmd;
            iss >> cmd;

            if (cmd == "/signup") {
                string id, pw; iss >> id >> pw;
                if (userManager.exists(id)) {
                    sendMessage(sock, "회원가입 실패: 중복된 아이디가 있습니다.\n");
                }
                else {
                    bool ok = userManager.signup(id, pw);
                    sendMessage(sock, ok ? "회원가입 성공\n" : "회원가입 실패\n");
                }
            }

            else if (cmd == "/login") {
                string id, pw; iss >> id >> pw;
                if (userManager.login(id, pw)) {
                    userManager.setOnline(id, true);
                    {
                        lock_guard<mutex> lock(onlineMutex);
                        onlineUsers[sock] = id;
                    }
                    currentUser = id;
                    sendMessage(sock, "로그인 성공\n");

                    // ✅ 입장 로그 출력
                    cout << ">> [서버] " << id << " 님이 입장했습니다.\n";
                    {
                        lock_guard<mutex> lock(onlineMutex);
                        printOnlineUsers();
                    }
                }
                else {
                    sendMessage(sock, "로그인 실패\n");
                }
            }
            else if (cmd == "/getfriends") {
                string res = "[내 친구 목록]\n";
                auto list = friendManager.getFriends(onlineUsers[sock]);
                for (auto& f : list)
                    res += f + (userManager.isOnline(f) ? " (온라인)\n" : " (오프라인)\n");
                if (list.empty()) res += "(친구 없음)\n";
                sendMessage(sock, res);
            }
            else if (cmd == "/friendrequest") {
                string target; iss >> target;
                string sender = onlineUsers[sock];
                if (target == sender || !userManager.exists(target) || !userManager.isOnline(target) || friendManager.isFriend(sender, target))
                    sendMessage(sock, "친구 요청 실패\n");
                else if (friendManager.hasRequest(target, sender))
                    sendMessage(sock, "이미 요청 보냄\n");
                else {
                    friendManager.request(sender, target);
                    sendMessage(sock, "친구 요청 보냄\n");
                }
            }
            else if (cmd == "/viewrequests") {
                string res = "[받은 친구 요청 목록]\n";
                auto reqs = friendManager.getRequests(onlineUsers[sock]);
                for (auto& r : reqs)
                    res += "- " + r + "\n";
                if (reqs.empty()) res += "(요청 없음)\n";
                sendMessage(sock, res);
            }
            else if (cmd == "/acceptfriend") {
                string from; iss >> from;
                friendManager.accept(onlineUsers[sock], from);
                sendMessage(sock, "수락 완료\n");
            }
            else if (cmd == "/entertalk") {
                lock_guard<mutex> lock(chatMutex);
                chatParticipants.insert(sock);
                sendMessage(sock, "/entertalk_ack\n");
            }
            else if (cmd == "/나가기" || cmd == "/종료") {
                lock_guard<mutex> lock(chatMutex);
                chatParticipants.erase(sock);
                sendMessage(sock, "[채팅방 퇴장 처리 완료]\n");
            }
            else if (cmd == "/귓속말" || cmd == "/whisper") {
                string target; iss >> target; string msg;
                getline(iss, msg); msg.erase(0, msg.find_first_not_of(" "));
                string sender = onlineUsers[sock];
                if (!userManager.exists(target) || !friendManager.isFriend(sender, target)) {
                    sendMessage(sock, "귓속말 실패\n");
                }
                else {
                    SOCKET targetSock = -1;
                    {
                        lock_guard<mutex> lock(onlineMutex);
                        for (auto& p : onlineUsers)
                            if (p.second == target) {
                                targetSock = p.first;
                                break;
                            }
                    }
                    if (targetSock != -1) {
                        string whisper = "[귓속말 from " + sender + "] " + msg + "\n";
                        sendMessage(sock, whisper);
                        sendMessage(targetSock, whisper);
                    }
                    else {
                        sendMessage(sock, "상대방 오프라인\n");
                    }
                }
            }
            else if (cmd == "/게임") {
                string sub; iss >> sub;
                if (sub == "스피드") {
                    lock_guard<mutex> lock(gameMutex);
                    if (speedGameActive)
                        sendMessage(sock, "[게임] 스피드 게임이 이미 진행 중입니다.\n");
                    else {
                        currentSpeedNumber = generateSpeedNumber();
                        speedWinner.clear();
                        speedGameActive = true;
                        speedStartTime = chrono::steady_clock::now();
                        broadcast("[게임] 숫자 입력: " + currentSpeedNumber + "\n");
                    }
                }
                else if (sub == "업다운") {
                    lock_guard<mutex> lock(gameMutex);
                    if (updownActive)
                        sendMessage(sock, "[게임] 업다운 게임이 이미 진행 중입니다.\n");
                    else {
                        updownAnswer = 1 + rand() % 100;
                        updownActive = true;
                        updownTries.clear();
                        broadcast("[게임] 업다운 게임 시작! (1~100 숫자 중 하나)\n /입력 [숫자] 형식으로 제출하세요!\n");
                    }
                }
            }
            else if (cmd == "/입력") {
                int guess; iss >> guess;
                string id = onlineUsers[sock];
                lock_guard<mutex> lock(gameMutex);
                if (!updownActive) sendMessage(sock, "[게임] 현재 업다운 게임이 없습니다.\n");
                else {
                    updownTries[id]++;
                    if (guess == updownAnswer) {
                        updownActive = false;
                        broadcast("[게임] 승자: " + id + " (" + to_string(updownTries[id]) + "회 시도)\n");
                    }
                    else if (guess < updownAnswer) sendMessage(sock, "[게임] 업!\n");
                    else sendMessage(sock, "[게임] 다운!\n");
                }
            }
            else {
                if (!currentUser.empty()) {
                    lock_guard<mutex> lock(gameMutex);
                    if (speedGameActive && cmdline == currentSpeedNumber && speedWinner.empty()) {
                        speedWinner = currentUser;
                        speedGameActive = false;
                        auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - speedStartTime).count();
                        broadcast("[게임] 승자: " + speedWinner + " (" + to_string(elapsed) + "ms)\n");
                        continue;
                    }
                    string msg = "[" + currentUser + "] " + cmdline + "\n";
                    broadcast(msg);

                    // ✅ 채팅 로그 출력
                    cout << ">> 보낸 사람: " << currentUser << "\n";
                    cout << ">> 내용: " << cmdline << "\n";
                    cout << ">> 보낸 시간: " << getCurrentTimeString() << "\n";
                    cout << "----------------------------------------------\n";
                }
            }
        }

        if (!currentUser.empty())
            userManager.setOnline(currentUser, false);
        {
            lock_guard<mutex> lock(onlineMutex);
            onlineUsers.erase(sock);

            // ✅ 퇴장 로그 출력
            cout << ">> [서버] " << currentUser << " 님이 퇴장했습니다.\n";
            printOnlineUsers();
        }
        {
            lock_guard<mutex> lock(chatMutex);
            chatParticipants.erase(sock);
        }
        closesocket(sock);
    }

public:
    void run(int port = 12345) {
        srand(static_cast<unsigned>(time(0)));
        if (WSAStartup(MAKEWORD(2, 2), new WSADATA) != 0) {
            cerr << "[에러] WSAStartup 실패\n";
            return;
        }

        userManager.load();
        friendManager.load();

        serverSock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            cerr << "[에러] 바인딩 실패\n";
            return;
        }

        if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR) {
            cerr << "[에러] 리스닝 실패\n";
            return;
        }

        cout << "서버 실행 중...\n";
        while (true) {
            SOCKET clientSock = accept(serverSock, nullptr, nullptr);
            if (clientSock != INVALID_SOCKET) {
                thread(&ChatServer::handleClient, this, clientSock).detach();
            }
        }
    }
};


int main() {
    ChatServer server;
    server.run(12345);
    return 0;
}