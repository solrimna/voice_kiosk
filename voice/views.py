from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework import status
from django.core.files.storage import default_storage
from django.core.files.base import ContentFile
from django.conf import settings
from openai import OpenAI

# LangChain imports
# LangChain 1.x에서는 메모리 기능이 코어에 내장되어있음
# LangChain 1.x부터는 langGraph로 Agent 를 만들라고 권장하기에 -> OpenAI 직접 사용으로 변경함
# from langchain_openai import ChatOpenAI
# from langchain_core.prompts import ChatPromptTemplate, MessagesPlaceholder
# from langchain_core.output_parsers import StrOutputParser
# from langchain_core.chat_history import InMemoryChatMessageHistory
# from langchain_core.runnables.history import RunnableWithMessageHistory

from menu.models import MenuItem
from .tools import search_menu, create_order, get_recent_orders
import os
import json
import traceback
from pathlib import Path
from datetime import datetime # 파일명 생성할 때 타임스탬프 쓰기 위해 

# 전역 대화 히스토리
conversation_history = []


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
    print(f"\n{'='*50}")
    print(f"저장 경로: {file_path}")

    try:
        # 2. OpenAI Whisper STT 호출 (음성 -> 텍스트)
        user_message = call_whisper_stt(file_path)
        print(f"STT 결과: {user_message}")

        # 3. AI 대화 처리 (OpenAI Function Calling)
        ai_response =chat_with_tools(user_message)
        print(f'AI 응답결과: {ai_response}')

        # 4. TTS (텍스트 → 음성) 
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
        
        print("응답 데이터:", response_data)  
        print("=" * 50) 
        
        return Response(response_data, status=status.HTTP_200_OK)
    except Exception as e:
        print(f"오류: {str(e)}")
        traceback.print_exc() # 상세 오류 콘솔 출력용
        return Response({
            'error': f'처리 실패: {str(e)}'
        }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

def call_whisper_stt(audio_file_path):
    """OpenAI Whisper API 호출 (STT)"""
    
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
    with client.audio.speech.with_streaming_response.create(
        model="tts-1",  # 또는 "tts-1-hd" (고품질, 2배 비용)
        voice="nova",   # alloy, echo, fable, onyx, nova, shimmer 중 선택
        input=text,
        speed=0.9       # 0.25 ~ 4.0 (시니어 위해 약간 느리게)
    ) as response :
        # 파일로 저장
        # stream_to_file -> 실제로 스트리밍을 안하는 버그가 있어서 경고가 뜸 -> with_streaming_response 추가
        response.stream_to_file(str(tts_file_path))        
    
    return str(tts_file_path)

def get_menu_context():
    """메뉴 DB에서 컨텍스트 생성"""
    
    menu_items = MenuItem.objects.filter(is_available=True)
    
    if not menu_items.exists():
        return "현재 판매 가능한 메뉴가 없습니다."
    
    menu_list = []
    
    for item in menu_items[:10]:  # 최대 10개
        # 기본 정보
        menu_info = f"• {item.name}: {item.price:,}원"
        
        # 태그 추가
        tags = []
        if item.is_hot:
            tags.append("HOT")
        if item.is_cold:
            tags.append("ICE")
        if not item.is_caffeine:
            tags.append("디카페인")
        
        if tags:
            menu_info += f" [{', '.join(tags)}]"
        
        # 설명 추가
        if item.description:
            menu_info += f"\n  └ {item.description}"
        
        # 카테고리
        menu_info += f" (종류: {item.get_category_display()})"
        
        menu_list.append(menu_info)
    
    return "\n".join(menu_list)

# ─── Function Calling 도구 정의 ───────────────────────────────────────

TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "search_menu",
            "description": "카페 메뉴를 검색합니다.",
            "parameters": {
                "type": "object",
                "properties": {
                    "keyword": {"type": "string", "description": "검색할 메뉴명"},
                    "is_cold": {"type": "boolean", "description": "차가운 음료 여부"},
                    "is_hot": {"type": "boolean", "description": "뜨거운 음료 여부"},
                    "is_caffeine": {"type": "boolean", "description": "카페인 포함 여부"}
                }
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "create_order",
            "description": "메뉴를 주문합니다. 명확한 주문 의사가 있을 때만 사용합니다.",
            "parameters": {
                "type": "object",
                "properties": {
                    "menu_name": {"type": "string", "description": "주문할 메뉴명"},
                    "quantity": {"type": "integer", "description": "수량", "default": 1}
                },
                "required": ["menu_name"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_recent_orders",
            "description": "최근 주문 내역을 조회합니다.",
            "parameters": {
                "type": "object",
                "properties": {
                    "limit": {"type": "integer", "description": "조회 개수", "default": 5}
                }
            }
        }
    }
]

SYSTEM_PROMPT = """
당신은 친절한 카페 키오스크 AI 주문 도우미입니다.

**역할:**
- 어르신도 편하게 이용할 수 있도록 쉽고 천천히 안내
- 음료 메뉴 추천 및 주문 도움
- 모호한 표현도 이해하고 친절하게 대응
- 필요시 도구를 호출하여 정확한 정보 제공

**현재 판매 중인 메뉴:**
{menu_list}

**사용 가능한 도구:**
- search_menu: 메뉴 검색 (키워드, 카테고리, 온도, 카페인)
- create_order: 주문 생성 (메뉴명, 수량)
- get_recent_orders: 최근 주문 조회

**대화 원칙:**
1. "뭐가 맛있어요?", "추천해줘요" → 위 메뉴에서 2-3개 구체적으로 추천
2. "따뜻한 거", "달지 않은 거" 등 선호도 언급 시 → 해당 옵션 고려하여 안내
3. "주세요", "할게요" 등 주문 의사 명확할 때만 → create_order 도구로 주문 확정
4. 옵션 선택 필요 시 (hot/ice 등) → 친절하게 여쭤보기
5. 주문 완료 전 반드시 내용 한 번 확인
6. 메뉴/주문과 무관한 질문 → "죄송합니다, 주문 관련 내용만 도와드릴 수 있어요. 직원을 불러드릴까요?"
7. 메뉴 검색이 필요하면 search_menu 도구 사용
8. 고객이 명확히 주문하면 create_order 도구로 즉시 처리
9. 주문 내역 질문 시 get_recent_orders 도구 사용
10. 도구 사용 결과를 자연스럽게 전달
         
**응답 스타일:**
- 짧고 명확하게 (2-3문장)
- 가격 정보 포함
- 이모티콘 사용 금지
- 존댓말, 부드러운 말투
"""

def execute_tool(tool_name: str, tool_args: dict) -> str:
    """도구 실행 라우터"""
    try:
        if tool_name == "search_menu":
            result = search_menu(**tool_args)
        elif tool_name == "create_order":
            result = create_order(**tool_args)
        elif tool_name == "get_recent_orders":
            result = get_recent_orders(**tool_args)
        else:
            result = json.dumps({"error": f"알 수 없는 도구: {tool_name}"}, ensure_ascii=False)
    except Exception as e:
        result = json.dumps({"error": f"도구 실행 오류: {str(e)}"}, ensure_ascii=False)
    
    return result


def chat_with_tools(user_input: str) -> str:
    """OpenAI Function Calling으로 대화 처리"""
    global conversation_history
    
    # 메뉴 컨텍스트 가져오기
    menu_context = get_menu_context()
    
    # SYSTEM_PROMPT에 메뉴 주입 > 동적으로 메뉴 리스트를 주입하기 위해서 
    system_prompt_with_menu = SYSTEM_PROMPT.format(menu_list=menu_context)

    # 사용자 메시지 추가
    conversation_history.append({"role": "user", "content": user_input})
    
    # 메시지 구성 (SYSTEM_PROMPT이 아니라 위에서 만든 system_prompt_with_menu 사용)
    messages = [{"role": "system", "content": system_prompt_with_menu}] + conversation_history
    
    # OpenAI API 호출
    client = OpenAI(api_key=settings.OPENAI_API_KEY)
    
    response = client.chat.completions.create(
        model="gpt-4o-mini",
        temperature=0.7,
        messages=messages,
        tools=TOOLS,
        tool_choice="auto"
    )
    
    message = response.choices[0].message
        
    # 도구 호출이 있는 경우
    if response.choices[0].finish_reason == "tool_calls":
        print(f"\n🔧 도구 호출:")
        
        conversation_history.append(message.model_dump())
        
        for tool_call in message.tool_calls:
            tool_name = tool_call.function.name
            tool_args = json.loads(tool_call.function.arguments)
            
            print(f"  Tool: {tool_name}")
            print(f"  Args: {tool_args}")
            
            tool_result = execute_tool(tool_name, tool_args)
            print(f"  Result: {tool_result}\n")
            
            conversation_history.append({
                "role": "tool",
                "tool_call_id": tool_call.id,
                "content": tool_result
            })
        
        # 최종 응답 생성
        final_response = client.chat.completions.create(
            model="gpt-4o-mini",
            temperature=0.7,
            messages=[{"role": "system", "content": SYSTEM_PROMPT}] + conversation_history
        )
        
        reply = final_response.choices[0].message.content
    else:
        reply = message.content
    
    conversation_history.append({"role": "assistant", "content": reply})
    
    print(f"  [누적 메시지: {len(conversation_history)}개]")
    
    return reply


@api_view(['POST'])
def process_text(request):
    """
    텍스트를 직접 받아서 처리 (STT 없이)
    Azure 실시간 STT에서 텍스트만 전송받을 때 사용
    """
    
    print(f"\n{'='*50}")
    print("📝 텍스트 직접 처리 시작")
    
    # 텍스트 가져오기
    user_message = request.data.get('text', '')
    
    if not user_message:
        return Response(
            {'error': '텍스트가 없습니다.'},
            status=status.HTTP_400_BAD_REQUEST
        )
    
    print(f"👤 사용자: {user_message}")
    
    try:
        # AI 대화 처리 
        ai_response = chat_with_tools(user_message)
        print(f"🤖 AI: {ai_response}")
        
        # TTS 
        tts_file_path = call_openai_tts(ai_response)
        print(f"🔊 TTS 파일 생성: {tts_file_path}")
        print(f"{'='*50}\n")
        
        # 음성 파일 URL 생성
        tts_file_name = os.path.basename(tts_file_path)
        tts_url = f"/media/tts/{tts_file_name}"
        
        return Response({
            'message': '처리 완료',
            'user_message': user_message,
            'ai_response': ai_response,
            'tts_url': tts_url
        }, status=status.HTTP_200_OK)
        
    except Exception as e:
        print(f"❌ 오류: {str(e)}")
        traceback.print_exc()
        return Response({
            'error': f'처리 실패: {str(e)}'
        }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
