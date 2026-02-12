from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework import status
from django.core.files.storage import default_storage
from django.core.files.base import ContentFile
from django.conf import settings
from openai import OpenAI
import os


@api_view(['POST'])
def upload_voice(request):
    """음성 파일 업로드 API"""
    
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

    # 2. OpenAI Whisper STT 호출
    try:
        transcription = call_whisper_stt(file_path)
        print(f"STT 결과: {transcription}")

        response_data = {
            'message': '음성 파일 업로드 성공',
            'file_name': voice_file.name,
            'file_path': file_path,
            'file_size': voice_file.size,
            'transcription': transcription
        }
        
        print("응답 데이터:", response_data)  # 디버그
        print("=" * 50)  # 디버그
        
        return Response(response_data, status=status.HTTP_201_CREATED)
    except Exception as e:
        print(f"STT 오류: {str(e)}")
        return Response({
            'error': f'STT 변환 실패: {str(e)}'
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