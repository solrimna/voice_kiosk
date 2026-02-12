from django.urls import path
from .views import upload_voice

urlpatterns = [
    path('upload/', upload_voice, name='upload_voice'),
]