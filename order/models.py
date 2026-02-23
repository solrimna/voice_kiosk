from django.db import models
from menu.models import MenuItem

class Order(models.Model):
    """주문"""
    menu = models.ForeignKey(MenuItem, on_delete=models.CASCADE, verbose_name="메뉴")
    quantity = models.IntegerField(default=1, verbose_name="수량")
    total_price = models.IntegerField(verbose_name="총 가격")
    
    STATUS_CHOICES = [
        ('pending', '대기'),
        ('confirmed', '확인'),
        ('completed', '완료'),
        ('cancelled', '취소'),
    ]
    status = models.CharField(
        max_length=20,
        choices=STATUS_CHOICES,
        default='pending',
        verbose_name="상태"
    )
    
    created_at = models.DateTimeField(auto_now_add=True)
    
    class Meta:
        verbose_name = "주문"
        verbose_name_plural = "주문들"
        ordering = ['-created_at']
    
    def __str__(self):
        return f"{self.menu.name} x {self.quantity}"
    
    def save(self, *args, **kwargs):
        # 총 가격 자동 계산
        self.total_price = self.menu.price * self.quantity
        super().save(*args, **kwargs)