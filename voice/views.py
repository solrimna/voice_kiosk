from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework import status
from django.core.files.storage import default_storage
from django.core.files.base import ContentFile
from django.conf import settings
from openai import OpenAI

# LangChain imports
# LangChain 1.x에서는 메모리 기능이 코어에 내장되어있음
from langchain_openai import ChatOpenAI
from langchain_core.prompts import ChatPromptTemplate, MessagesPlaceholder
from langchain_core.output_parsers import StrOutputParser
from langchain_core.chat_history import InMemoryChatMessageHistory
from langchain_core.runnables.history import RunnableWithMessageHistory

from menu.models import MenuItem
import os
import traceback
from pathlib import Path
from datetime import datetime # 파일명 생성할 때 타임스탬프 쓰기 위해 

# 전역 메모리 스토어 
store = {}

# LangChain 1.x에서는 세션별 메모리가 관리된다. 
def get_session_history(session_id: str):
    """세션별 메모리 가져오기"""
    if session_id not in store:
        store[session_id] = InMemoryChatMessageHistory()
    return store[session_id]

@api_view(['POST'])
def upload_voice(request):
    """음성 파일 업로드 + STT + AI 대화 + TTS """
    
    print("=" * 50)  
    print("요청 받음!")  
    print("FILES:", request.FILES)  

    # 파일 확인
    if 'voice_file' not in request.FILES:
        print("에러: voice_file 없음")
        return Response(
            {'error': '음성 파일이 없습니다.'}, 
            status=status.HTTP_400_BAD_REQUEST
        )
    
    voice_file = request.FILES['voice_file']
    print(f"파일명: {voice_file.name}")  
    print(f"파일 크기: {voice_file.size}") 

    # 1. 파일 저장
    file_name = default_storage.save(
        f'voices/{voice_file.name}',
        ContentFile(voice_file.read())
    )
    file_path = default_storage.path(file_name)
    print(f"저장 경로: {file_path}")

    try:
        # 2. OpenAI Whisper STT 호출 (음성 -> 텍스트)
        user_message = call_whisper_stt(file_path)
        print(f"STT 결과: {user_message}")

        # 3. AI 대화 처리 (LangChain)
        ai_response = process_conversation_with_langchain(user_message)
        print(f'AI 응답결과: {ai_response}')

        # 4. TTS (텍스트 → 음성) - 새로 추가!
        tts_file_path = call_openai_tts(ai_response)
        print(f"🔊 TTS 파일 생성: {tts_file_path}")
        print(f"{'='*50}\n")
        
        # 5. 음성 파일 URL 생성
        tts_file_name = os.path.basename(tts_file_path)
        tts_url = f"/media/tts/{tts_file_name}"

        response_data = {
            'message': '성공',
            # 'file_name': voice_file.name,
            # 'file_path': file_path,
            # 'file_size': voice_file.size,
            'user_message': user_message,
            'ai_response': ai_response,
            'tts_url': tts_url # TTS 음성 파일 URL 추가 
        }
        
        print("응답 데이터:", response_data)  # 디버그
        print("=" * 50)  # 디버그
        
        return Response(response_data, status=status.HTTP_200_OK)
    except Exception as e:
        print(f"STT 오류: {str(e)}")
        traceback.print_exc() # 상세 오류 콘솔 출력용
        return Response({
            'error': f'처리 실패: {str(e)}'
        }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

def call_whisper_stt(audio_file_path):
    """OpenAI Whisper API 호출"""
    
    client = OpenAI(api_key=settings.OPENAI_API_KEY)
    
    print(f"Whisper API 호출 시작: {audio_file_path}")
    
    # 음성 파일 열기
    with open(audio_file_path, 'rb') as audio_file:
        # Whisper API 호출
        transcription = client.audio.transcriptions.create(
            model="whisper-1",
            file=audio_file,
            language="ko"  # 한국어 명시 (인식률 향상)
        )
    
    print(f"Whisper API 응답: {transcription.text}")
    return transcription.text

def call_openai_tts(text):
    """OpenAI TTS API 호출 (텍스트 → 음성)"""
    
    client = OpenAI(api_key=settings.OPENAI_API_KEY)
    
    # TTS 폴더 생성
    tts_dir = Path(settings.MEDIA_ROOT) / 'tts'
    tts_dir.mkdir(parents=True, exist_ok=True)
    
    # 파일명 생성 (타임스탬프)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    tts_file_path = tts_dir / f"response_{timestamp}.mp3"
    
    # TTS API 호출
    response = client.audio.speech.create(
        model="tts-1",  # 또는 "tts-1-hd" (고품질, 2배 비용)
        voice="nova",   # alloy, echo, fable, onyx, nova, shimmer 중 선택
        input=text,
        speed=0.9       # 0.25 ~ 4.0 (시니어 위해 약간 느리게)
    )
    
    # 파일로 저장
    response.stream_to_file(str(tts_file_path))
    
    return str(tts_file_path)

def get_menu_context():
    """메뉴 DB에서 컨텍스트 생성"""
    
    menu_items = MenuItem.objects.filter(is_available=True)
    
    if not menu_items.exists():
        return "현재 판매 가능한 메뉴가 없습니다."
    
    menu_list = []
    
    for item in menu_items:
        # 기본 정보
        menu_info = f"• {item.name}: {item.price:,}원"
        
        # 태그 추가
        tags = []
        if item.is_spicy:
            tags.append("매운맛")
        if item.is_cold:
            tags.append("차가움")
        if item.is_hot:
            tags.append("뜨거움")
        
        if tags:
            menu_info += f" [{', '.join(tags)}]"
        
        # 설명 추가
        if item.description:
            menu_info += f"\n  └ {item.description}"
        
        # 카테고리
        menu_info += f" (종류: {item.get_category_display()})"
        
        menu_list.append(menu_info)
    
    return "\n".join(menu_list)


def process_conversation_with_langchain(user_message):
    """LangChain으로 대화 처리"""
    
    # 1. 메뉴 컨텍스트 가져오기
    menu_context = get_menu_context()
    
    print(f"\n📋 메뉴 컨텍스트:\n{menu_context}\n")
    
    # 2. 프롬프트 템플릿 (System + History + User)
    prompt = ChatPromptTemplate.from_messages([
        ("system", """당신은 친절한 음식점 키오스크 AI 주문 도우미입니다.

**역할:**
- 시니어(어르신) 고객을 배려한 쉽고 따뜻한 말투
- 메뉴 추천 및 주문 도움
- 모호한 표현도 이해하고 적절히 대응

**현재 판매 중인 메뉴:**
{menu_list}

**대화 원칙:**
1. 고객이 "뭐 먹지?", "뭐 사지?", "추천해줘" 등 물으면 → 메뉴 2-3개 구체적으로 추천
2. "매운 거", "차가운 거" 등 선호도 언급 시 → 해당 태그 고려하여 필터링
3. "주세요", "할게요" 등 주문 의사 명확할 때만 → 주문 확정
4. 불명확한 경우 → 친절하게 재질문

**응답 스타일:**
- 짧고 명확하게 (2-3문장)
- 가격 정보 포함
- 이모티콘 사용 금지
- 존댓말 사용"""),
        MessagesPlaceholder(variable_name="chat_history"),   # 대화 히스토리 들어갈 자리
        ("user", "{input}")                                  # 사용자 입력
    ])
    
    # 3. LLM 설정
    llm = ChatOpenAI(
        model="gpt-4o-mini",
        temperature=0.7,
        api_key=settings.OPENAI_API_KEY
    )
    
    # 4. 체인 구성 (LangChain 1.x 방식)
    chain = prompt | llm | StrOutputParser()
    
    # 5. 메모리 포함 체인 
    chain_with_history = RunnableWithMessageHistory(
        chain,
        get_session_history,
        input_messages_key="input",
        history_messages_key="chat_history"                 #  프롬프트의 MessagesPlaceholder와 동일해야
    )
    
    # 6. 대화 실행
    response = chain_with_history.invoke(
        {
            "input": user_message,
            "menu_list": menu_context
        },
        config={"configurable": {"session_id": "default"}}
    )

    # 메모리 확인 (디버깅)
    print(f"\n💾 현재 메모리:")
    history = get_session_history("default")
    print(f"대화 개수: {len(history.messages)}")
    for msg in history.messages:
        role = "사용자" if msg.type == "human" else "AI"
        print(f"  {role}: {msg.content[:50]}...")
    print()
    
    return response