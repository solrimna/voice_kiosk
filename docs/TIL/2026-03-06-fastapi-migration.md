# 2026.03.06 - FastAPI 마이그레이션

## 📅 날짜
2026년 3월 6일 (목)

## 🎯 목표
Django 백엔드를 FastAPI로 전환하여 FastAPI 수업 배운 내용 응용

## 📚 오늘 실습해 본 것
### step1. FastAPI 기본
```
다 변경하지는 않고, FastAPI + Django ORM으로~

django view.py에 있던 함수 -> fast api로 처리하기위해 main.py로 옮김
    but, 단순 옮기기 사용시 Exception 발생!, 
    "detail": "처리 실패: You cannot call this from an async context - use a thread or sync_to_async."
```
### step2. 비동기 처리 반영
```
# 비동기로 변경 시켜주기 위해 Django 함수 래핑(sync_to_async 사용)
ai_response = chat_with_tools(user_message)
-> ai_response = await sync_to_async(chat_with_tools)(request.text)
```

### step3. Dependencies & Yield 패턴도 배웠으니 응용
```
Depends & Yield 를 배운대로 db 연결 관리를 반영하기에는
이미 구현한 소스가 django ORM을 이용하므로(django ORM가 알아서 연결 관리), 
클라이언트 연결 로그에 응용하는 방향으로...

def log_request():
    # ① 시작
    start = datetime.now()
    print("📥 요청 시작")
    
    yield request_id  # ② 여기서 멈춤!
    
    # ⑥ 다시 돌아옴!
    elapsed = datetime.now() - start
    print(f"⏱️ {elapsed}초")

@app.post("/api/...")
async def endpoint(id = Depends(log_request)):
    # ③ yield가 준 값 받음
    # ④ 실제 작업
    result = process()
    # ⑤ return → yield 이후 실행!
    return result
```

### step4. 기타 - 시스템 프롬프트 추가 수정
``` 메뉴를 조회했더니 넘버링한 결과가 너무 길어서 답변 음성 기다리는게 너무 오래걸림

문제 :
AI: "차가운 음료로는 다음과 같은 메뉴가 있습니다:
1. 아이스 아메리카노 - 4,500원 (시원한...)
2. 아이스 카페라떼 - 5,000원 (시원한...)..."

수정 : 시스템 프롬프트 조건 추가 
- 명확한 규칙 제시 (번호 금지 등)
- 예시로 보여주기

```