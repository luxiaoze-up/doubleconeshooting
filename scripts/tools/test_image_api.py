"""
Test script for Image Stream API
"""

import requests
import base64
import json
import time

API_BASE = "http://localhost:8080/api"

def test_health():
    """Test health endpoint"""
    print("Testing /api/health...")
    try:
        resp = requests.get(f"{API_BASE}/health")
        print(f"Status: {resp.status_code}")
        print(f"Response: {json.dumps(resp.json(), indent=2)}")
        return resp.status_code == 200
    except Exception as e:
        print(f"Error: {e}")
        return False

def test_ccd_status(camera_id):
    """Test CCD status endpoint"""
    print(f"\nTesting /api/ccd/{camera_id}/status...")
    try:
        resp = requests.get(f"{API_BASE}/ccd/{camera_id}/status")
        print(f"Status: {resp.status_code}")
        print(f"Response: {json.dumps(resp.json(), indent=2)}")
        return resp.status_code == 200
    except Exception as e:
        print(f"Error: {e}")
        return False

def test_ccd_image(camera_id):
    """Test CCD image endpoint"""
    print(f"\nTesting /api/ccd/{camera_id}/image...")
    try:
        resp = requests.get(f"{API_BASE}/ccd/{camera_id}/image")
        print(f"Status: {resp.status_code}")
        data = resp.json()
        if "image" in data:
            img_size = len(base64.b64decode(data["image"]))
            print(f"Image size: {img_size} bytes")
            print(f"Format: {data.get('format')}")
            print(f"Timestamp: {data.get('timestamp')}")
        else:
            print(f"Response: {json.dumps(data, indent=2)}")
        return resp.status_code == 200
    except Exception as e:
        print(f"Error: {e}")
        return False

def test_all_status():
    """Test all CCD status endpoint"""
    print("\nTesting /api/ccd/all/status...")
    try:
        resp = requests.get(f"{API_BASE}/ccd/all/status")
        print(f"Status: {resp.status_code}")
        print(f"Response: {json.dumps(resp.json(), indent=2)}")
        return resp.status_code == 200
    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    print("=" * 50)
    print("Image Stream API Test")
    print("=" * 50)
    
    # Test health
    if not test_health():
        print("\nHealth check failed. Is the API server running?")
        exit(1)
    
    # Test all CCD status
    test_all_status()
    
    # Test individual cameras
    cameras = ["upperCCD1x", "upperCCD10x", "lowerCCD1x", "lowerCCD10x"]
    for camera_id in cameras:
        test_ccd_status(camera_id)
        test_ccd_image(camera_id)
        time.sleep(0.5)
    
    print("\n" + "=" * 50)
    print("Test completed!")
    print("=" * 50)

