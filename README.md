## 추가 수정하기
```
현재 방식 : 녹음완료 -> 전송 -> 처리
수정 필요 : 말하는 중 -> 실시간 진행
```

## 현재 동작 플로우
```
👤 사용자: "뭐 있어요?" (음성)
    ↓
🎤 [Qt] 녹음 (WAV 파일)
    ↓
📤 [Qt → Django] 파일 업로드
    ↓
🎧 [Whisper STT] "뭐 있어요?" (텍스트 변환)
    ↓
🧠 [LangChain + GPT-4o-mini]
    - 메뉴 DB 참조
    - 이전 대화 기억 💾
    - "물냉면, 비빔밥 있습니다" (응답 생성)
    ↓
🔊 [OpenAI TTS] MP3 파일 생성
    ↓
📥 [Django → Qt] TTS URL 전달
    ↓
🔉 [Qt] 음성 재생
    ↓
👂 사용자: AI 음성 듣기!
```

## 🎬 현재까지의 데모 시나리오 

### 시나리오 1: 메뉴 추천
```
사용자: "오늘 날씨 더운데 뭐 먹을까요?"
AI 음성: "더운 날씨에는 시원한 물냉면이나 콩국수 추천드립니다!"
```

### 시나리오 2: 선호도 고려
```
사용자: "매운 거 말고 뭐 있어요?"
AI 음성: "매운 거 빼고는 물냉면, 콩국수, 비빔밥 있습니다."
```

### 시나리오 3: 연속 대화
```
사용자: "뭐 있어요?"
AI 음성: "물냉면, 비빔밥 있습니다."

사용자: "물냉면 주세요"
AI 음성: "물냉면 1개 주문하시겠어요?"
```

## 🛠️ 사용된 기술 스택
Frontend (Qt C++)

Qt 6.10.2
Qt Multimedia (녹음/재생)
Qt Network (HTTP 통신)
QMediaPlayer (TTS 재생)

Backend (Django Python)

Django 6.0.2(>5.x)
Django REST Framework
PostgreSQL/SQLite

AI/ML

OpenAI Whisper (STT)
OpenAI GPT-4o-mini (대화)
OpenAI TTS (음성 합성)
LangChain 1.2.10 (메모리, 체인) (>1.x)

인프라

.env 환경 변수 관리
Media 파일 서빙
CORS 설정

## 이용 라이브러리 
1. python-dotenv            : .env사용을 위해 추가
2. openai                   : OpenAI API 사용을 위한 추가
3. langchain                : langchain 뼈대
4. langchain-openai         : langchain에 openai 연결하기 위한 추가 
5. langchain-community      : langchain 확장 도구
