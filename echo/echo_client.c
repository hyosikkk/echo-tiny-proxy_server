#include <stdio.h>  // 표준 입출력 함수 사용을 위한 헤더
#include "csapp.h"

int open_clientfd(char *hostname, char*port);   // 클라이언트 소켓을 여는 함수 선언

int main(int argc, char **argv) {
    int clientfd;                       // 서버와의 연결 소켓 파일 디스크립터
    char *host, *port, buf[MAXLINE];    // 호스트명, 포트번호, 입출력 버퍼
    rio_t rio;                          // robust I/O를 위한 구조체

    if (argc != 3){                     // 인자 개수가 정확하지 않으면
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);  // 사용법 출력
        exit(0);                         // 프로그램 종료
    }
    host = argv[1];         // 호스트 이름 (예: "localhost")
    port = argv[2];         // 포트 번호 (예: "8080")

    clientfd = open_clientfd(host, port);       // 서버에 연결하고 연결 소켓을 반환받음
    Rio_readinitb(&rio, clientfd);              // rio 구조체를 초기화 (소켓에 대한 읽기 버퍼 설정)

    while (Fgets(buf, MAXLINE, stdin) != NULL){     // 사용자로부터 한 줄 입력을 받을 동안 반복
        Rio_writen(clientfd, buf, strlen(buf));     // 입력한 문자열을 서버로 보냄
        Rio_readlineb(&rio, buf, MAXLINE);          // 서버가 보낸 응답 한 줄을 읽음
        Fputs(buf, stdout);                         // 읽은 응답을 화면에 출력
    }
    Close(clientfd);                                // 서버와의 연결 소켓을 닫음
    exit(0);                                        // 프로그램 종료
}

int open_clientfd(char *hostname, char*port) {
    int clientfd;                                   // 연결 소켓 디스크립터
    struct addrinfo hints, *listp, *p;              // 주소 정보 저장 구조체

    memset(&hints, 0, sizeof(struct addrinfo));     // hints 구조체 초기화 (0으로 채움)
    hints.ai_socktype = SOCK_STREAM;                // TCP 사용 (스트림 소켓)
    hints.ai_flags = AI_NUMERICSERV;                // 포트를 숫자로만 처리
    hints.ai_flags |= AI_ADDRCONFIG;                // 로컬에 적절한 주소만 반환 (IPv4/IPv6 자동 선택)
    Getaddrinfo(hostname, port, &hints, &listp);    // 호스트와 포트에 대한 주소 리스트를 받아옴

    for (p = listp; p; p = p->ai_next){             // 주소 리스트를 순회하면서
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;                               // 소켓 생성 실패하면 다음 주소로
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break;                                  // connect 성공하면 루프 탈출
        Close(clientfd);                            // connect 실패 시 소켓 닫음
    }
    
    Freeaddrinfo(listp);                            // 동적 할당된 주소 정보 해제
    if (!p)                                         // 모든 연결 실패 시
        return -1;                                  // 실패 값 반환
    else
        return clientfd;                            // 성공한 소켓 디스크립터 반환
}
