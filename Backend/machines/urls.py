from django.urls import path, include
from rest_framework.routers import DefaultRouter
from . import views

router = DefaultRouter()
router.register(r'machines', views.VendingMachineViewSet)

# urlpatterns = [
#     path('', include(router.urls)),
    
# ]

from django.urls import path
from . import views
from django.conf import settings
from django.conf.urls.static import static

urlpatterns = [
    path('', views.MachineListView.as_view(), name='machine_list'),
    path('machine/<int:pk>/', views.MachineDetailView.as_view(), name='machine_detail'),
    path('api/', include(router.urls)),
    path('api/machines/', views.VendingMachineViewSet.as_view({'get': 'list'})),
    path('api/machines/<int:pk>/', views.VendingMachineViewSet.as_view({'get': 'retrieve'})),
   path('api/machines/<str:machine_id>/quality-history/', 
         views.VendingMachineViewSet.as_view({'get': 'quality_history'}),
         name='machine-quality-history'),
         
]+ static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)
