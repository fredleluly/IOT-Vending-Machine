from django.shortcuts import render

# Create your views here.
from rest_framework import viewsets
from rest_framework.decorators import action
from rest_framework.response import Response
from django.utils import timezone
from .models import VendingMachine, WaterQuality, SalesRecord
from .serializers import (
    VendingMachineSerializer, 
    WaterQualitySerializer,
    SalesRecordSerializer
)


from django.views.generic import ListView, DetailView
from .models import VendingMachine

from django.views.generic import ListView, DetailView
from django.utils import timezone
from datetime import timedelta
from .models import VendingMachine

class VendingMachineViewSet(viewsets.ModelViewSet):
    queryset = VendingMachine.objects.all()
    serializer_class = VendingMachineSerializer
    filterset_fields = ['status', 'location']
    lookup_field = 'machine_id'

    @action(detail=True, methods=['post'])
    def record_quality(self, request,  machine_id=None):
        try:
            machine = VendingMachine.objects.get(machine_id=machine_id)
            serializer = WaterQualitySerializer(data=request.data)
            
            if serializer.is_valid():
                serializer.save(machine=machine)
                return Response(serializer.data)
            return Response(serializer.errors, status=400)
            
        except VendingMachine.DoesNotExist:
            return Response({"error": "Machine not found"}, status=404)

    # @action(detail=True, methods=['post'])
    # def record_quality(self, request, pk=None):
    #     machine = self.get_object()
    #     serializer = WaterQualitySerializer(data=request.data)
        
    #     if serializer.is_valid():
    #         serializer.save(machine=machine)
    #         return Response(serializer.data)
    #     return Response(serializer.errors, status=400)

    @action(detail=True, methods=['post'])
    def record_sale(self, request, pk=None):
        machine = self.get_object()
        serializer = SalesRecordSerializer(data=request.data)
        
        if serializer.is_valid():
            serializer.save(machine=machine)
            return Response(serializer.data)
        return Response(serializer.errors, status=400)

    @action(detail=True, methods=['get'])
    def quality_history(self, request, machine_id=None):
        try:
            machine = VendingMachine.objects.get(machine_id=machine_id)
            
            # Default ambil 24 jam terakhir
            end_date = timezone.now()
            start_date = end_date - timedelta(hours=24)
            
            # Bisa filter by range
            if 'start_date' in request.query_params:
                start_date = timezone.datetime.fromisoformat(request.query_params['start_date'])
            if 'end_date' in request.query_params:
                end_date = timezone.datetime.fromisoformat(request.query_params['end_date'])
            
            qualities = machine.water_qualities.filter(
                timestamp__range=(start_date, end_date)
            ).order_by('timestamp')
            
            serializer = WaterQualitySerializer(qualities, many=True)
            return Response(serializer.data)
            
        except VendingMachine.DoesNotExist:
            return Response({"error": "Machine not found"}, status=404)

# class MachineListView(ListView):
#     model = VendingMachine
#     template_name = 'machines/machine_list.html'
#     context_object_name = 'machines'

    # def get_context_data(self, **kwargs):
    #     context = super().get_context_data(**kwargs)
    #     for machine in context['machines']:
    #         # Ambil water quality terbaru untuk setiap machine
    #         latest_quality = machine.water_qualities.first()  # Karena sudah di-order by -timestamp
    #         machine.latest_quality = latest_quality
    #     return context

# views.py
from django.db.models import Q

class MachineListView(ListView):
    model = VendingMachine
    template_name = 'machines/machine_list.html'
    context_object_name = 'machines'
    paginate_by = 6  # Menampilkan 12 machines per page
    
    def get_queryset(self):
        queryset = VendingMachine.objects.all()
        
        # Search functionality
        search = self.request.GET.get('search', '')
        if search:
            queryset = queryset.filter(
                Q(name__icontains=search) |
                Q(location__icontains=search) |
                Q(machine_id__icontains=search)
            )
            
        # Filter by status
        status = self.request.GET.get('status', '')
        if status:
            queryset = queryset.filter(status=status)
            
        return queryset
    
    def get_context_data(self, **kwargs):
        context = super().get_context_data(**kwargs)
        # Add extra context
        context['search'] = self.request.GET.get('search', '')
        context['status'] = self.request.GET.get('status', '')
        context['total_machines'] = VendingMachine.objects.count()
        context['online_machines'] = VendingMachine.objects.filter(status='online').count()
        for machine in context['machines']:
            # Ambil water quality terbaru untuk setiap machine
            latest_quality = machine.water_qualities.first()  # Karena sudah di-order by -timestamp
            machine.latest_quality = latest_quality
        return context

class MachineDetailView(DetailView):
    model = VendingMachine
    template_name = 'machines/machine_detail.html'
    context_object_name = 'machine'

    def get_context_data(self, **kwargs):
        context = super().get_context_data(**kwargs)
        machine = self.get_object()
        
        # Get latest water quality
        latest_quality = machine.water_qualities.first()
        machine.latest_quality = latest_quality
        
        # Get today's sales
        today = timezone.now().date()
        context['total_sales_today'] = machine.sales.filter(
            timestamp__date=today
        ).count()

        return context
    

