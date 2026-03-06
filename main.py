from fastapi import FastAPI, HTTPException, Depends, Header
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
import os
import sys
from asgiref.sync import sync_to_async  # 비동기 래핑 위해 추가 
from datetime import datetime           # 로깅 위해 추가 
from typing import Optional             # 로깅 위해 추가 

# Django ORM 사용을 위한 설정
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'kiosk_server.settings')

import django
django.setup()

# Django 함수들 import
from voice.views import chat_with_tools, call_openai_tts

# FastAPI 앱 생성
app = FastAPI(
    title="음성 AI 카페 키오스크 API",
    description="실시간 음성 주문 시스템 (FastAPI + Azure Speech + OpenAI)",
    version="2.0.0"
)

# CORS 설정
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Qt 클라이언트 허용
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 정적 파일 (TTS 음성)
media_path = os.path.join(os.path.dirname(__file__), "media")
if os.path.exists(media_path):
    app.mount("/media", StaticFiles(directory=media_path), name="media")


# ─── 로깅 추가 ────────────────────────────────────
def log_request():
    """
    요청 로깅 (yield 패턴)
    
    요청 시작/종료 시간을 자동으로 로깅합니다.
    """
    start = datetime.now()
    request_id = start.strftime("%Y%m%d_%H%M%S_%f")
    
    print(f"\n{'='*60}")
    print(f"📥 요청 ID: {request_id}")
    print(f"⏰ 시작 시각: {start.strftime('%Y-%m-%d %H:%M:%S')}")
    
    yield request_id  
    
    # 요청 완료 후 실행
    end = datetime.now()
    elapsed = (end - start).total_seconds()
    
    print(f"⏱️  처리 시간: {elapsed:.2f}초")
    print(f"✅ 완료 시각: {end.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'='*60}\n")

def get_client_info(
    user_agent: Optional[str] = Header(None),
    x_forwarded_for: Optional[str] = Header(None)
):
    """
    클라이언트 정보 추출 (yield 패턴)
    
    User-Agent, IP 등을 추출합니다.
    """
    client_info = {
        "user_agent": user_agent or "Unknown",
        "ip": x_forwarded_for or "Unknown"
    }
    
    print(f"🖥️  클라이언트: {client_info['user_agent'][:50]}")
    print(f"🌐 IP: {client_info['ip']}")
    
    yield client_info
    
    # 요청 완료 후 (필요시 추가 로깅)
    print(f"👋 클라이언트 연결 종료")


# ─── Pydantic 모델 ────────────────────────────────────

class TextRequest(BaseModel):
    text: str

class ApiResponse(BaseModel):
    message: str
    user_message: str
    ai_response: str
    tts_url: str

# ─── API 엔드포인트 ────────────────────────────────────

@app.get("/")
async def root():
    """API 루트"""
    return {
        "message": "음성 AI 카페 키오스크 API",
        "version": "2.0.0",
        "docs": "/docs",
    }

@app.post("/api/voice/process_text/", response_model=ApiResponse)
async def process_text(
    request: TextRequest,
    request_id: str = Depends(log_request),           # 로깅
    client_info: dict = Depends(get_client_info),     # 클라이언트 정보
):
    """
    실시간 STT 텍스트 처리(STT 없이)
    
    - Azure Speech SDK에서 실시간으로 인식된 텍스트 수신
    - OpenAI Function Calling으로 AI 처리
    - TTS 음성 생성 및 URL 반환
    - 자동 요청 로깅 (시작/종료 시간)
    """
    
    print(f"\n{'='*50}")
    print(f"📝 텍스트 처리 시작")
    print(f"👤 사용자: {request.text}")
    
    if not request.text or not request.text.strip():
        raise HTTPException(status_code=400, detail="텍스트가 비어있습니다.")
    
    try:
        # AI 대화 처리
        # step1. django view.py에 있던 함수 -> fast api로 처리하기위해 main.py로 옮김
        #           but, 단순 옮기기 사용시 Exception 발생!, "detail": "처리 실패: You cannot call this from an async context - use a thread or sync_to_async."
        # step2. 비동기로 변경 시켜주기 위해 래핑 시켜준다 (sync_to_async 사용)
        ai_response = await sync_to_async(chat_with_tools)(request.text)
        print(f"🤖 AI: {ai_response}")
        
        # TTS 생성 
        tts_file_path = await sync_to_async(call_openai_tts)(ai_response)
        print(f"🔊 TTS: {tts_file_path}")
        print(f"{'='*50}\n")
        
        # 음성 파일 URL 생성
        tts_file_name = os.path.basename(tts_file_path)
        tts_url = f"/media/tts/{tts_file_name}"
        
        return ApiResponse(
            message="처리 완료",
            user_message=request.text,
            ai_response=ai_response,
            tts_url=tts_url
        )
        
    except Exception as e:
        print(f"❌ 오류: {str(e)}")
        import traceback
        traceback.print_exc()
        raise HTTPException(status_code=500, detail=f"처리 실패: {str(e)}")

@app.get("/api/menu/items/")
async def get_menu_items(
    request_id: str = Depends(log_request),
    client_info: dict = Depends(get_client_info)
):
    """메뉴 목록 조회"""
    from menu.models import MenuItem
    
    # 메뉴도 변경 - sync_to_async 래핑 
    #   그냥 못감쌈!, Django ORM 쿼리셋 -> list로
    items = await sync_to_async(list)(MenuItem.objects.all())
    return {
        "count": len(items),
        "items": [
            {
                "id": item.id,
                "name": item.name,
                "category": item.category,
                "price": item.price,
                "is_hot": item.is_hot,
                "is_cold": item.is_cold,
                "is_caffeine": item.is_caffeine,
                "is_available": item.is_available
            }
            for item in items
        ]
    }

@app.get("/health")
async def health_check():
    """헬스 체크"""
    return {"status": "healthy"}

# ─── 서버 실행 (개발용) ─────────────────────────────────

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "main:app",
        host="127.0.0.1",
        port=8000,
        reload=True,  # 코드 변경 시 자동 재시작
        log_level="info"
    )