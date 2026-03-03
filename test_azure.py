import os
from dotenv import load_dotenv
import azure.cognitiveservices.speech as speechsdk

load_dotenv()

# 설정
speech_config = speechsdk.SpeechConfig(
    subscription=os.getenv('AZURE_SPEECH_KEY'),
    region=os.getenv('AZURE_SPEECH_REGION')
)
speech_config.speech_recognition_language = "ko-KR"

print("✅ Azure Speech 설정 완료!")
print(f"지역: {speech_config.region}")
print(f"언어: {speech_config.speech_recognition_language}")

# 마이크 테스트
audio_config = speechsdk.audio.AudioConfig(use_default_microphone=True)
recognizer = speechsdk.SpeechRecognizer(
    speech_config=speech_config,
    audio_config=audio_config
)

print("\n🎤 말씀하세요 (5초 안에)...")
result = recognizer.recognize_once()

if result.reason == speechsdk.ResultReason.RecognizedSpeech:
    print(f"\n✅ 인식 성공: {result.text}")
elif result.reason == speechsdk.ResultReason.NoMatch:
    print("\n❌ 음성 인식 실패")
elif result.reason == speechsdk.ResultReason.Canceled:
    cancellation = result.cancellation_details
    print(f"\n❌ 취소됨: {cancellation.reason}")
    if cancellation.reason == speechsdk.CancellationReason.Error:
        print(f"에러: {cancellation.error_details}")