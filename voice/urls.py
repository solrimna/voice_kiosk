from django.urls import path
from .views import upload_voice, process_text

urlpatterns = [
    path('upload/', upload_voice, name='upload_voice'),
    path('process_text/', process_text, name='process_text'),
]