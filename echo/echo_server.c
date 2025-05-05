#include <stdio.h>
#include "csapp.h"

int open_listenfd(char *port);      // 클라이언트의 연결을 기다릴 소켓 생성 함수 선언
void echo(int connfd);              // 클라이언트의 연결을 기다릴 소켓 생성 함수 선언

int main(int argc, char **argv) {
	int listenfd, connfd;                                   // 서버 소켓과 연결된 클라이언트 소켓 디스크립터
    socklen_t clientlen;                                    // 클라이언트 주소 길이
    struct sockaddr_storage clientaddr;                     // 클라이언트 주소 정보 저장 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE];    // 클라이언트 호스트 이름과 포트 번호 저장

    // 포트 번호 인자가 없으면 에러 메시지 출력 후 종료
    if (argc !=2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = open_listenfd(argv[1]);      // 서버가 들을 소켓 생성, Open_listenfd()는 CSAPP 책에서 제공하는 에러 처리를 포함한 래퍼 함수
    while(1){
        clientlen = sizeof(struct sockaddr_storage);        // 주소 구조체의 크기 초기화
        // 클라이언트의 연결 요청을 accept해서 연결된 소켓(connfd) 획득
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // 클라이언트 주소 정보를 호스트 이름과 포트 문자열로 변환
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        // 연결된 클라이언트 정보 출력
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);       // 클라이언트와 데이터 주고받기 (echo 기능)
        Close(connfd);      // 연결 종료
    }
    exit(0);                // 프로그램 종료 (여기 도달하지 않음)
}

int open_listenfd(char *port) {
    struct addrinfo hints, *listp, *p;          // 주소 정보를 위한 구조체들
    int listenfd, optval=1;                     // 소켓 디스크립터와 옵션 값 설정

    memset(&hints, 0, sizeof(struct addrinfo));     // hints 구조체 초기화
    hints.ai_socktype = SOCK_STREAM;                // TCP 소켓 사용
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;    // 수신용 주소, 적절한 주소만 사용
    hints.ai_flags |= AI_NUMERICSERV;               // 포트는 숫자로 해석

    // 포트에 대한 주소 정보 리스트 생성 (호스트는 NULL = 현재 호스트)
    Getaddrinfo(NULL, port, &hints, &listp);

    // 주소 리스트 순회하면서 소켓 생성 시도
    for (p = listp; p; p = p->ai_next){
        // 소켓 생성 (실패하면 다음 후보로)
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;
        // SO_REUSEADDR 옵션 설정 → 포트를 바로 재사용 가능
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

        // 바인딩 성공하면 루프 종료
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) ==0)
            break;
        Close(listenfd);    // 실패한 소켓은 닫기
    }

    Freeaddrinfo(listp);    // 동적으로 할당된 주소 정보 해제
    if (!p)                 // 바인딩에 실패한 경우
        return -1;
    
    // 소켓을 수신 대기 상태로 변경
    if (listen(listenfd, LISTENQ) < 0) {
        Close(listenfd);
        return -1;
    }
    return listenfd;        // 생성한 리슨 소켓 디스크립터 반환
}

void echo(int connfd) {
    size_t n;               // 읽은 바이트 수
    char buf[MAXLINE];      // 버퍼
    rio_t rio;              // Robust I/O 구조체

    Rio_readinitb(&rio, connfd);    // rio 버퍼를 connfd와 연결
    // 클라이언트로부터 한 줄씩 읽고
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) !=0){
        printf("server received %d bytes\n", (int)n);       // 받은 바이트 수 출력
        Rio_writen(connfd, buf, n);                         // 읽은 그대로 다시 클라이언트에 전송
    }
}