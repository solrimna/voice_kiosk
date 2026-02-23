from django.db import models

class MenuItem(models.Model):
    """메뉴 아이템"""
    name = models.CharField(max_length=100, verbose_name="메뉴명")
    price = models.IntegerField(verbose_name="가격")
    description = models.TextField(blank=True, verbose_name="설명")
    image = models.ImageField(upload_to='menu_images/', blank=True, null=True, verbose_name="이미지")
    
    # 태그 
    is_hot = models.BooleanField(default=False, verbose_name="HOT 가능")
    is_cold = models.BooleanField(default=False, verbose_name="ICE 가능")
    is_caffeine = models.BooleanField(default=True, verbose_name="카페인 포함")
    
    category = models.CharField(
        max_length=20,
        choices=[
            ('coffee', '커피'),
            ('latte', '라떼'),
            ('smoothie', '스무디'),
            ('tea', '티'),
            ('dessert', '디저트'),
        ],
        verbose_name="카테고리"
    )
    
    is_available = models.BooleanField(default=True, verbose_name="판매 가능")
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)
    
    class Meta:
        verbose_name = "메뉴"
        verbose_name_plural = "메뉴들"
    
    def __str__(self):
        return self.name