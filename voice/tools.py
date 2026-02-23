from menu.models import MenuItem
from order.models import Order
from typing import Optional
import json

# ─── 1단계: 실제 함수 구현 ────────────────────────────────────────────────────

def search_menu(keyword: Optional[str] = None, category: Optional[str] = None, 
                is_cold: Optional[bool] = None, is_hot: Optional[bool] = None,
                is_caffeine: Optional[bool] = None) -> str:
    """
    메뉴 검색 함수
    
    Args:
        keyword: 검색 키워드 (메뉴명)
        category: 카테고리 (coffee, latte, smoothie, tea, dessert)
        is_cold: 차가운 음료 여부
        is_hot: 뜨거운 음료 여부
        is_caffeine: 카페인 포함 여부
    
    Returns:
        검색된 메뉴 목록 (JSON 문자열)
    """
    queryset = MenuItem.objects.filter(is_available=True)
    
    if keyword:
        queryset = queryset.filter(name__icontains=keyword)
    
    if category:
        queryset = queryset.filter(category=category)
    
    if is_cold is not None:
        queryset = queryset.filter(is_cold=is_cold)
    
    if is_hot is not None:
        queryset = queryset.filter(is_hot=is_hot)
    
    if is_caffeine is not None:
        queryset = queryset.filter(is_caffeine=is_caffeine)
    
    if not queryset.exists():
        return json.dumps({"error": "검색 결과가 없습니다."}, ensure_ascii=False)
    
    menus = []
    for item in queryset[:5]:
        menus.append({
            "name": item.name,
            "price": item.price,
            "description": item.description,
            "category": item.get_category_display()
        })
    
    return json.dumps({"menus": menus}, ensure_ascii=False)


def create_order(menu_name: str, quantity: int = 1) -> str:
    """
    주문 생성 함수
    
    Args:
        menu_name: 메뉴명
        quantity: 수량
    
    Returns:
        주문 결과 (JSON 문자열)
    """
    try:
        menu = MenuItem.objects.filter(
            name__icontains=menu_name,
            is_available=True
        ).first()
        
        if not menu:
            return json.dumps(
                {"error": f"'{menu_name}' 메뉴를 찾을 수 없습니다."},
                ensure_ascii=False
            )
        
        order = Order.objects.create(
            menu=menu,
            quantity=quantity
        )
        
        result = {
            "success": True,
            "menu_name": menu.name,
            "quantity": quantity,
            "total_price": order.total_price
        }
        return json.dumps(result, ensure_ascii=False)
    
    except Exception as e:
        return json.dumps(
            {"error": f"주문 처리 중 오류: {str(e)}"},
            ensure_ascii=False
        )


def get_recent_orders(limit: int = 5) -> str:
    """
    최근 주문 조회 함수
    
    Args:
        limit: 조회할 주문 개수
    
    Returns:
        최근 주문 목록 (JSON 문자열)
    """
    orders = Order.objects.all()[:limit]
    
    if not orders.exists():
        return json.dumps({"orders": []}, ensure_ascii=False)
    
    order_list = []
    for order in orders:
        order_list.append({
            "menu_name": order.menu.name,
            "quantity": order.quantity,
            "total_price": order.total_price,
            "status": order.get_status_display()
        })
    
    return json.dumps({"orders": order_list}, ensure_ascii=False)