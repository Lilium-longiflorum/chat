# chat

멀티플랫폼 채팅 프로그램입니다. 또한 기말고사는 사람을 미치게 만듭니다.

## 구성

* **server** – C로 작성된 채팅 서버 (Windows/Linux 지원)
* **client** – 콘솔 기반 클라이언트 (중단됨)
* **gui\_client** – Windows API를 활용한 GUI 클라이언트
* **ws\_bridge / bridge.js** – 웹소켓-소켓 브릿지 (Node.js)
* **web\_client** – 웹 브라우저 기반 클라이언트 (GitHub Pages에 배포)

## 빌드 방법

### 서버 (Windows)

```bash
gcc chat_server.c -o server.exe -lws2_32 -lpthread
```

### 서버 (Linux)

```bash
gcc -o server server.c -lpthread
```

### GUI 클라이언트 (Windows)

```bash
gcc gui_client.c -o gui_client.exe -lws2_32 -mwindows
```

> ※ `-mwindows` 옵션은 콘솔 창 없이 실행하기 위한 것입니다.

### 웹소켓 브리지 (Node.js)

```bash
node bridge.js
```

> `bridge.js`는 웹 클라이언트를 지원하기 위한 중계 서버입니다. 내부적으로 웹소켓을 TCP로 포워딩합니다.

## 실행 순서

1. 서버 실행
2. 필요시 `bridge.js` 실행 (웹 클라이언트 사용 시)
3. 원하는 클라이언트 실행

   * `gui_client.exe`
   * 웹 브라우저에서 GitHub Pages 접속

## 클라이언트 목록

* ✅ GUI 클라이언트 (Windows 앱)([address]:8080)
* ✅ 웹 클라이언트 (서버 실행시 나오는 주소)([address]:9000)
* ❌ 콘솔 클라이언트 (중단됨)

## 예시 화면

```
[allcho]: 안녕하세요!
[lily]: 안녕하세요, 웹입니다.
[System]: allcho left the chat
```

## 기능
* 닉네임 설정(서버로 보내는 첫 메시지를 닉네임으로 간주 - 공백 제거됨)
