#include <iostream>
#include <string>
#include <thread>
#include <limits>
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

class ChatClient {
private:
    SOCKET clientSock;
    bool connected;
    bool inChatRoom;
    HANDLE hConsole;
    string userId;

public:
    ChatClient() : connected(false), inChatRoom(false) {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    ~ChatClient() {
        closesocket(clientSock);
        WSACleanup();
    }

    void printWithColor(const string& msg, int color = 7) {
        SetConsoleTextAttribute(hConsole, color);
        cout << msg << flush;
        SetConsoleTextAttribute(hConsole, 7);
    }

    void showMessageWithPause(const string& msg, int color = 7, int milliseconds = 1000) {
        printWithColor(msg, color);
        Sleep(milliseconds);
    }


    void sendCommand(const string& command) {
        send(clientSock, command.c_str(), static_cast<int>(command.length()), 0);
        Sleep(100);
    }

    void receiveResponse() {
        char buffer[1024] = { 0 };
        int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            showMessageWithPause(buffer);
        }
        else {
            showMessageWithPause("[에러] 응답 수신 실패\n", 12);
        }
    }

    void receiveMessages() {
        char buffer[1024];
        while (connected && inChatRoom) {
            int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                string msg(buffer);

                if (msg.find("[" + userId + "]") == 0) continue;

                if (msg.find("[귓속말") != string::npos) {
                    printWithColor(msg, 13);
                }
                else if (msg.find("[게임]") != string::npos) {
                    printWithColor(msg, 10);
                }
                else if (msg[0] == '[') {
                    size_t endBracket = msg.find(']');
                    if (endBracket != string::npos) {
                        string sender = msg.substr(1, endBracket - 1);
                        string content = msg.substr(endBracket + 1);
                        content.erase(0, content.find_first_not_of(" "));

                        // ✅ 줄바꿈 제거
                        if (!content.empty() && content.back() == '\n') {
                            content.pop_back();
                        }

                        time_t now = time(nullptr);
                        struct tm t;
                        localtime_s(&t, &now);
                        char timeStr[20];
                        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &t);

                        cout << "----------------------------------------------\n";
                        cout << ">> 보낸 사람: " << sender << "\n";
                        cout << ">> 내용: " << content << "\n";
                        cout << ">> 보낸 시간: " << timeStr << "\n";
                        cout << "----------------------------------------------\n";
                    }
                }
                else {
                    cout << msg;
                }
            }
            else {
                connected = false;
                printWithColor("[서버 연결 종료됨]\n", 12);
                break;
            }
        }
    }





    void chatRoom() {
        sendCommand("/entertalk");

        char buffer[1024] = { 0 };
        int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            if (string(buffer).find("/entertalk_ack") != string::npos) {
                inChatRoom = true;
                printWithColor("\n[채팅방 입장 완료 '/나가기' 입력 시 퇴장, '/귓속말 [ID] [메시지]', '/게임 스피드', '/게임 업다운']\n", 11);
            }
            else {
                printWithColor("[채팅방 입장 실패]\n", 12);
                return;
            }
        }
        else {
            printWithColor("[에러] 채팅방 응답 없음\n", 12);
            return;
        }

        thread recvThread(&ChatClient::receiveMessages, this);

        while (connected && inChatRoom) {
            string msg;
            getline(cin, msg);

            if (msg == "/나가기" || msg == "/종료") {
                send(clientSock, msg.c_str(), static_cast<int>(msg.length()), 0);
                inChatRoom = false;
                break;
            }

            if (!msg.empty()) {
                send(clientSock, msg.c_str(), static_cast<int>(msg.length()), 0);
            }
        }

        if (recvThread.joinable()) recvThread.join();

        printWithColor("\n[채팅방에서 나갔습니다. 메인 메뉴로 돌아갑니다.]\n", 14);
        Sleep(1000);
    }

    void handleFriendRequests() {
        sendCommand("/viewrequests");
        char buffer[1024] = { 0 };
        int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            cout << buffer << flush;
            if (string(buffer).find("요청 없음") == string::npos) {
                cout << "수락할 ID 입력 (취소하려면 엔터): ";
                string acceptId;
                getline(cin, acceptId);
                if (!acceptId.empty()) {
                    sendCommand("/acceptfriend " + acceptId);
                    receiveResponse();
                }
            }
        }
        else {
            cout << "[에러] 친구 요청 수신 실패\n";
        }
    }

    void friendMenu() {
        while (connected) {
            system("cls");
            cout << "  *************************************************\n";
            cout << "  *                                               *\n";
            cout << "  *         *****   *   *    ***    *****         *\n";
            cout << "  *        *        *   *   *   *     *           *\n";
            cout << "  *        *        *****   *****     *           *\n";
            cout << "  *        *        *   *   *   *     *           *\n";
            cout << "  *         *****   *   *   *   *     *           *\n";
            cout << "  *                                               *\n";
            cout << "  *                                               *\n";
            cout << "  *                < 친구 메뉴 >                  *\n";
            cout << "  *                                               *\n";
            cout << "  *                1. 친구 목록 보기              *\n";
            cout << "  *                2. 친구 요청 보내기            *\n";
            cout << "  *                3. 친구 요청 확인/수락         *\n";
            cout << "  *                4. 메인 메뉴로 돌아가기        *\n";
            cout << "  *                                               *\n";
            cout << "  *************************************************\n";
            cout << "선택: ";
            int sub;
            cin >> sub;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (sub == 1) {
                sendCommand("/getfriends");
                receiveResponse();
                system("pause");
            }
            else if (sub == 2) {
                string target;
                cout << "요청할 친구 ID: ";
                getline(cin, target);
                sendCommand("/friendrequest " + target);
                receiveResponse();
                system("pause");
            }
            else if (sub == 3) {
                handleFriendRequests();
                system("pause");
            }
            else if (sub == 4) {
                break;
            }
            else {
                cout << "잘못된 선택입니다.\n";
                system("pause");
            }
        }
    }

    void mainMenu() {
        while (connected) {
            system("cls");
            cout << "  *************************************************\n";
            cout << "  *                                               *\n";
            cout << "  *         *****   *   *    ***    *****         *\n";
            cout << "  *        *        *   *   *   *     *           *\n";
            cout << "  *        *        *****   *****     *           *\n";
            cout << "  *        *        *   *   *   *     *           *\n";
            cout << "  *         *****   *   *   *   *     *           *\n";
            cout << "  *                                               *\n";
            cout << "  *                                               *\n";
            cout << "  *                < 메인 메뉴 >                  *\n";
            cout << "  *                                               *\n";
            cout << "  *                1. 친구 메뉴                   *\n";
            cout << "  *                2. 채팅방 입장                 *\n";
            cout << "  *                3. 종료                        *\n";
            cout << "  *                                               *\n";
            cout << "  *                                               *\n";
            cout << "  *************************************************\n";
            cout << "선택: ";
            int choice;
            cin >> choice;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (choice == 1) {
                friendMenu();
            }
            else if (choice == 2) {
                chatRoom();
            }
            else if (choice == 3) {
                cout << "종료합니다.\n";
                closesocket(clientSock);
                WSACleanup();
                exit(0);
            }
            else {
                cout << "잘못된 선택입니다.\n";
                system("pause");
            }
        }
    }

    void loginMenu() {
        while (connected) {
            system("cls");
            cout << "  *************************************************\n";
            cout << "  *                                               *\n";
            cout << "  *         *****   *   *    ***    *****         *\n";
            cout << "  *        *        *   *   *   *     *           *\n";
            cout << "  *        *        *****   *****     *           *\n";
            cout << "  *        *        *   *   *   *     *           *\n";
            cout << "  *         *****   *   *   *   *     *           *\n";
            cout << "  *                                               *\n";
            cout << "  *                                               *\n";
            cout << "  *                < 시작 메뉴 >                  *\n";
            cout << "  *                                               *\n";
            cout << "  *                1. 로그인                      *\n";
            cout << "  *                2. 회원가입                    *\n";
            cout << "  *                3. 종료                        *\n";
            cout << "  *                                               *\n";
            cout << "  *                                               *\n";
            cout << "  *************************************************\n";
            cout << "선택: ";
            int choice;
            cin >> choice;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (choice == 3) {
                connected = false;
                closesocket(clientSock);
                WSACleanup();
                exit(0);
            }

            string id, pw;
            cout << "ID: "; getline(cin, id);
            cout << "PW: "; getline(cin, pw);

            userId = id;

            string cmd = (choice == 1 ? "/login " : "/signup ") + id + " " + pw;
            sendCommand(cmd);

            char buffer[1024] = { 0 };
            int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                if (choice == 1 && string(buffer).find("성공") != string::npos) {
                    showMessageWithPause(buffer);  // 로그인 성공
                    mainMenu();
                    return;
                }
                else {
                    showMessageWithPause(buffer);  // 로그인 실패 또는 회원가입 실패
                }
            }
            else {
                showMessageWithPause("[에러] 서버 응답 없음\n", 12);
            }
        }
    }

    bool connectToServer(const string& ip, int port) {
        clientSock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        if (connect(clientSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            cout << "서버 연결 실패\n";
            return false;
        }

        connected = true;
        cout << "[클라이언트] 서버 연결됨\n";
        return true;
    }
};

int main() {
    ChatClient client;
    string ip;
    int port;
    cout << "서버 IP: "; cin >> ip;
    cout << "포트: "; cin >> port;
    if (client.connectToServer(ip, port)) {
        client.loginMenu();
    }
    return 0;
}