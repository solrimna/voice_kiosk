#from django.shortcuts import render
from rest_framework import viewsets
from .models import MenuItem
from .serializers import MenuItemSerializer

class MenuItemViewSet(viewsets.ModelViewSet):
    """메뉴 CRUD API"""
    queryset = MenuItem.objects.filter(is_available=True)
    serializer_class = MenuItemSerializer