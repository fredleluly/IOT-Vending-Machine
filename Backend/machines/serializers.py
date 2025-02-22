from rest_framework import serializers
from .models import VendingMachine, WaterQuality, SalesRecord

class WaterQualitySerializer(serializers.ModelSerializer):
    class Meta:
        model = WaterQuality
        fields = ['id', 'tds_level', 'ph_level', 'water_level', 'timestamp']

class SalesRecordSerializer(serializers.ModelSerializer):
    class Meta:
        model = SalesRecord
        fields = ['id', 'volume', 'price', 'timestamp']

class VendingMachineSerializer(serializers.ModelSerializer):
    latest_quality = serializers.SerializerMethodField()
    total_sales_today = serializers.SerializerMethodField()

    class Meta:
        model = VendingMachine
        fields = ['id', 'machine_id', 'name', 'location', 'status', 
                 'last_maintenance', 'installation_date', 'latest_quality',
                 'total_sales_today']

    def get_latest_quality(self, obj):
        latest = obj.water_qualities.first()
        if latest:
            return WaterQualitySerializer(latest).data
        return None

    def get_total_sales_today(self, obj):
        from django.utils import timezone
        from django.db.models import Sum
        today = timezone.now().date()
        return obj.sales.filter(
            timestamp__date=today
        ).aggregate(Sum('volume'))['volume__sum'] or 0