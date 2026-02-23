# 🎤 음성 AI 카페 키오스크

시니어 친화적 음성 주문 시스템 (Qt C++ + Django + OpenAI)

## 📋 프로젝트 개요

**목표:** 시니어가 쉽게 사용할 수 있는 음성 AI 카페 키오스크  
**기간:** 미완 / 계속 진행 중

**차별화:**
- 🎤 완전한 음성 인터페이스 (STT + AI + TTS)
- 🧠 대화 메모리 (이전 대화 기억)
- 🛠️ Function Calling (AI가 직접 주문 처리)
- 👴 시니어 친화적 UX

---
## 🚀 현재 동작 플로우
```
👤 사용자: "아메리카노 2잔 주세요" (음성)
    ↓
🎤 [Qt] 녹음 (WAV 파일)
    ↓
📤 [Qt → Django] 파일 업로드
    ↓
🎧 [Whisper STT] "아메리카노 2잔 주세요" (텍스트 변환)
    ↓
🧠 [OpenAI GPT-4o-mini + Function Calling]
    - 대화 히스토리 참조 💾
    - 도구 판단: create_order 필요
    ↓
🔧 [create_order 도구 실행]
    - 메뉴 DB 검색
    - 주문 DB 저장
    - 결과 반환: {"success": true, "total_price": 9000}
    ↓
🧠 [GPT-4o-mini] 
    - 도구 결과 해석
    - 응답 생성: "아메리카노 2잔 주문 완료! 9,000원입니다."
    ↓
🔊 [OpenAI TTS] MP3 파일 생성
    ↓
📥 [Django → Qt] TTS URL 전달
    ↓
🔉 [Qt] 음성 재생
    ↓
👂 사용자: AI 음성 듣기!
```
---
**Last Updated:** 2026.02.23
## 🔧 추가 예정 기능

### Stage 2: Azure Speech (실시간 STT)
```
현재 방식: 녹음 완료 → 전송 → 처리 (7-10초)
수정 예정: 말하는 중 → 실시간 진행 (1-2초)
```

**계획:**
- Azure Speech SDK (C++ 네이티브)
- WebSocket 실시간 스트리밍
- 음성 파형 시각화
- 자동 음성 감지 (버튼 불필요)

### UI/UX 개선
- [ ] 메뉴 이미지 표시
- [ ] 주문 내역 리스트
- [ ] 로딩 애니메이션
- [ ] 에러 처리 강화

---

## 🎬 데모 시나리오

### 시나리오 1: 메뉴 검색 (Function Calling)
```
사용자: "차가운 거 있어요?"

AI 동작:
  1. search_menu(is_cold=True) 도구 호출 🔧
  2. 차가운 음료 목록 받음
  3. "아이스 아메리카노, 아이스 카페라떼... 있습니다!"
```

### 시나리오 2: 주문 생성 (Function Calling)
```
사용자: "아메리카노 2잔 주세요"

AI 동작:
  1. create_order(menu_name="아메리카노", quantity=2) 호출 🔧
  2. DB에 주문 저장
  3. "아메리카노 2잔 주문 완료! 총 9,000원입니다!"
```

### 시나리오 3: 연속 대화 (메모리)
```
첫 번째:
사용자: "뭐 있어요?"
AI: "아메리카노, 카페라떼, 카푸치노 등 있습니다."

두 번째:
사용자: "아메리카노 주세요"
AI: "아메리카노 1잔 주문하시겠어요?" (이전 대화 기억!)
```

### 시나리오 4: 주문 조회 (Function Calling)
```
사용자: "내가 뭐 주문했지?"

AI 동작:
  1. get_recent_orders() 도구 호출 🔧
  2. 최근 주문 내역 조회
  3. "아메리카노 2잔 주문하셨습니다!"
```

---

## 🛠️ 기술 스택

### Frontend (Qt C++)
- **Qt 6.10.2** - 크로스플랫폼 UI
- **Qt Multimedia** - 오디오 녹음/재생
- **Qt Network** - HTTP 통신
- **QMediaPlayer** - TTS 음성 재생
- **MinGW 13.1.0** - C++ 컴파일러

### Backend (Django Python)
- **Django 5.1** - 웹 프레임워크
- **Django REST Framework** - REST API
- **SQLite** - 데이터베이스
- **CORS Headers** - 크로스 오리진 처리

### AI/ML
- **OpenAI Whisper** - STT (음성→텍스트)
- **OpenAI GPT-4o-mini** - AI 대화 + Function Calling
- **OpenAI TTS** - 음성 합성 (텍스트→음성)
- ~~**LangChain 1.2.10**~~ - OpenAI 네이티브로 전환

### 인프라
- **python-dotenv** - 환경 변수 관리
- **.env** - API 키 보안
- **Media 파일 서빙** - TTS 음성 파일
- **CORS 설정** - Qt ↔ Django 통신

---

## 📦 주요 라이브러리

### Python (Django)
```bash
Django==5.1
djangorestframework==3.14.0
django-cors-headers==4.3.1
openai==1.12.0
python-dotenv==1.0.0
```

**제거된 라이브러리:**
- ~~langchain==1.2.10~~ (OpenAI 네이티브로 전환)
- ~~langchain-openai==0.2.14~~
- ~~langchain-community==0.3.15~~

**이유:** LangChain 1.x에서 AgentExecutor 제거됨 → OpenAI Function Calling 직접 사용

---

## 🎯 핵심 기능

### 1. Function Calling (도구 사용)
**AI가 스스로 판단해서 함수 실행!**

**정의된 도구:**
- `search_menu`: 메뉴 검색 (키워드, 카테고리, 온도, 카페인)
- `create_order`: 주문 생성 (메뉴명, 수량)
- `get_recent_orders`: 최근 주문 조회

**구현 방식:**
```python
# voice/tools.py - 실제 함수 구현
def create_order(menu_name: str, quantity: int = 1) -> str:
    # DB에 주문 저장
    order = Order.objects.create(menu=menu, quantity=quantity)
    return json.dumps({"success": True, "total_price": order.total_price})

# voice/views.py - OpenAI Function Calling
TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "create_order",
            "description": "메뉴를 주문합니다",
            "parameters": {...}
        }
    }
]
```

### 2. 대화 메모리
**이전 대화 기억!**
```python
# OpenAI 네이티브 메모리 (간단!)
conversation_history = []

conversation_history.append({"role": "user", "content": "뭐 있어요?"})
conversation_history.append({"role": "assistant", "content": "물냉면..."})
```

### 3. 30개 카페 메뉴 DB
**fixtures로 한 번에 로드!**
```bash
python manage.py loaddata fixtures/menu_data.json
```

---

## 📂 프로젝트 구조
```
KioskBackend/                   # Django 백엔드
├── fixtures/
│   └── menu_data.json         # 30개 카페 메뉴 데이터
├── kiosk_server/
│   ├── settings.py            # Django 설정
│   └── urls.py
├── menu/
│   ├── models.py              # MenuItem 모델
│   └── admin.py
├── order/
│   ├── models.py              # Order 모델
│   └── admin.py
├── voice/
│   ├── views.py               # OpenAI Function Calling
│   └── tools.py               # 도구 함수들
├── media/
│   ├── voices/                # 녹음 파일
│   └── tts/                   # TTS 음성 파일
├── .env                       # API 키 (Git 제외!)
├── .gitignore
└── requirements.txt

Kiosk_Step1/                    # Qt 프론트엔드
├── CMakeLists.txt
├── main.cpp
├── mainwindow.h
├── mainwindow.cpp
└── mainwindow.ui
```

---

## 🚀 실행 방법

### 1. Django 백엔드 실행
```bash
cd KioskBackend

# 가상환경 활성화
.venv\Scripts\activate  # Windows
source .venv/bin/activate  # Mac/Linux

# 패키지 설치
pip install -r requirements.txt

# .env 파일 생성
OPENAI_API_KEY=sk-proj-your-key-here

# 마이그레이션
python manage.py migrate

# 관리자 계정 생성
python manage.py createsuperuser

# 메뉴 데이터 로드
python manage.py loaddata fixtures/menu_data.json

# 서버 실행
python manage.py runserver
```

### 2. Qt 프론트엔드 실행
```
1. Qt Creator 실행
2. CMakeLists.txt 열기
3. Configure Project (MinGW 64-bit 선택)
4. Ctrl+R (빌드 & 실행)
```

### 3. 테스트
```
1. "녹음 시작" 클릭
2. "아메리카노 주세요" 말하기
3. "녹음 중지" 클릭
4. AI 음성 응답 듣기!
```

---

## 📊 API 엔드포인트
```
POST /api/voice/upload/
  - 음성 파일 업로드
  - STT → AI 대화 → TTS
  - 응답: {user_message, ai_response, tts_url}

GET /api/menu/items/
  - 메뉴 목록 조회

POST /admin/
  - Django 관리자 페이지
```

---

## 🎓 학습 포인트

### AI 특강 적용 (2026.02.19)
- **Function Calling** 구현 (도구 정의 + 실행 라우터 분리)
- **OpenAI 네이티브 방식** (LangChain 의존성 제거)
- **대화 메모리** (conversation_history)
- **도구 실행 결과를 AI가 자연스럽게 전달**

### 기술 전환
- **LangChain 1.x 문제:** AgentExecutor 제거됨
- **해결:** OpenAI Function Calling 직접 사용
- **특강 스타일:** 도구 정의 + 함수 구현 + 라우터 분리
---

## 💡 참고 자료

**OpenAI:**
- Whisper: https://platform.openai.com/docs/guides/speech-to-text
- Function Calling: https://platform.openai.com/docs/guides/function-calling
- TTS: https://platform.openai.com/docs/guides/text-to-speech

**Qt:**
- Qt 6 Docs: https://doc.qt.io/qt-6/
- Qt Multimedia: https://doc.qt.io/qt-6/qtmultimedia-index.html

**Django:**
- Django REST Framework: https://www.django-rest-framework.org/

---
