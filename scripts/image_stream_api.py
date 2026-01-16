"""
Image Stream REST API Service
Provides REST API endpoints for CCD camera image streaming
"""

from flask import Flask, jsonify, send_file, Response, request
from flask_cors import CORS
import tango
import base64
import io
import threading
import time
from PIL import Image
import os

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

# Configuration
DEVICE_NAME = "sys/reflection_imaging/1"
API_PORT = 8080
IMAGE_CACHE_DIR = "./images"
CACHE_DURATION = 5  # seconds

# Global state
device_proxy = None
image_cache = {}  # camera_id -> (image_data, timestamp)
cache_lock = threading.Lock()


def connect_device():
    """Connect to Tango device"""
    global device_proxy
    try:
        device_proxy = tango.DeviceProxy(DEVICE_NAME)
        device_proxy.ping()
        print(f"Connected to Tango device: {DEVICE_NAME}")
        return True
    except Exception as e:
        print(f"Failed to connect to Tango device: {e}")
        return False


def fetch_image_from_device(camera_id):
    """Fetch image from Tango device"""
    if not device_proxy:
        return None
        
    try:
        # Map camera ID to command name
        cmd_map = {
            "upperCCD1x": "getUpperCCD1xImage",
            "upperCCD10x": "getUpperCCD10xImage",
            "lowerCCD1x": "getLowerCCD1xImage",
            "lowerCCD10x": "getLowerCCD10xImage",
        }
        
        cmd_name = cmd_map.get(camera_id)
        if not cmd_name:
            return None
            
        result = device_proxy.command_inout(cmd_name)
        if result:
            # Try to decode as base64
            try:
                if isinstance(result, str):
                    img_data = base64.b64decode(result)
                else:
                    img_data = result
                return img_data
            except:
                # If not base64, try as file path
                if isinstance(result, str) and os.path.exists(result):
                    with open(result, 'rb') as f:
                        return f.read()
        return None
    except Exception as e:
        print(f"Error fetching image for {camera_id}: {e}")
        return None


def update_image_cache():
    """Background thread to update image cache"""
    cameras = ["upperCCD1x", "upperCCD10x", "lowerCCD1x", "lowerCCD10x"]
    
    while True:
        for camera_id in cameras:
            img_data = fetch_image_from_device(camera_id)
            if img_data:
                with cache_lock:
                    image_cache[camera_id] = (img_data, time.time())
        time.sleep(0.1)  # 10 FPS update rate


@app.route('/api/health', methods=['GET'])
def health():
    """Health check endpoint"""
    return jsonify({
        "status": "ok",
        "device_connected": device_proxy is not None,
        "device_name": DEVICE_NAME
    })


@app.route('/api/ccd/<camera_id>/image', methods=['GET'])
def get_ccd_image(camera_id):
    """Get latest image from specified CCD camera"""
    # Check cache first
    with cache_lock:
        if camera_id in image_cache:
            img_data, timestamp = image_cache[camera_id]
            if time.time() - timestamp < CACHE_DURATION:
                # Return cached image
                img_b64 = base64.b64encode(img_data).decode('utf-8')
                return jsonify({
                    "camera_id": camera_id,
                    "image": img_b64,
                    "format": "base64",
                    "timestamp": timestamp
                })
    
    # Fetch fresh image
    img_data = fetch_image_from_device(camera_id)
    if img_data:
        # Update cache
        with cache_lock:
            image_cache[camera_id] = (img_data, time.time())
        
        img_b64 = base64.b64encode(img_data).decode('utf-8')
        return jsonify({
            "camera_id": camera_id,
            "image": img_b64,
            "format": "base64",
            "timestamp": time.time()
        })
    else:
        return jsonify({
            "error": "Failed to fetch image",
            "camera_id": camera_id
        }), 500


@app.route('/api/ccd/<camera_id>/image/raw', methods=['GET'])
def get_ccd_image_raw(camera_id):
    """Get latest image as raw binary (for direct display)"""
    # Check cache first
    with cache_lock:
        if camera_id in image_cache:
            img_data, timestamp = image_cache[camera_id]
            if time.time() - timestamp < CACHE_DURATION:
                return Response(img_data, mimetype='image/png')
    
    # Fetch fresh image
    img_data = fetch_image_from_device(camera_id)
    if img_data:
        # Update cache
        with cache_lock:
            image_cache[camera_id] = (img_data, time.time())
        
        return Response(img_data, mimetype='image/png')
    else:
        return jsonify({"error": "Failed to fetch image"}), 500


@app.route('/api/ccd/<camera_id>/status', methods=['GET'])
def get_ccd_status(camera_id):
    """Get status of specified CCD camera"""
    if not device_proxy:
        return jsonify({"error": "Device not connected"}), 500
        
    try:
        attr_name = f"{camera_id}State"
        state = device_proxy.read_attribute(attr_name).value
        
        # Get additional info
        exposure_attr = f"{camera_id}Exposure"
        exposure = None
        try:
            exposure = device_proxy.read_attribute(exposure_attr).value
        except:
            pass
            
        return jsonify({
            "camera_id": camera_id,
            "state": state,
            "exposure": exposure,
            "timestamp": time.time()
        })
    except Exception as e:
        return jsonify({
            "error": str(e),
            "camera_id": camera_id
        }), 500


@app.route('/api/ccd/all/status', methods=['GET'])
def get_all_ccd_status():
    """Get status of all CCD cameras"""
    cameras = ["upperCCD1x", "upperCCD10x", "lowerCCD1x", "lowerCCD10x"]
    status = {}
    
    for camera_id in cameras:
        try:
            resp = get_ccd_status(camera_id)
            if resp.status_code == 200:
                status[camera_id] = resp.get_json()
            else:
                status[camera_id] = {"error": "Failed to get status"}
        except Exception as e:
            status[camera_id] = {"error": str(e)}
    
    return jsonify(status)


@app.route('/api/ccd/<camera_id>/capture', methods=['POST'])
def capture_ccd_image(camera_id):
    """Trigger image capture for specified CCD camera"""
    if not device_proxy:
        return jsonify({"error": "Device not connected"}), 500
        
    try:
        cmd_map = {
            "upperCCD1x": "captureUpperCCD1xImage",
            "upperCCD10x": "captureUpperCCD10xImage",
            "lowerCCD1x": "captureLowerCCD1xImage",
            "lowerCCD10x": "captureLowerCCD10xImage",
        }
        
        cmd_name = cmd_map.get(camera_id)
        if not cmd_name:
            return jsonify({"error": "Invalid camera_id"}), 400
            
        result = device_proxy.command_inout(cmd_name)
        return jsonify({
            "camera_id": camera_id,
            "result": str(result),
            "timestamp": time.time()
        })
    except Exception as e:
        return jsonify({
            "error": str(e),
            "camera_id": camera_id
        }), 500


@app.route('/api/ccd/<camera_id>/exposure', methods=['GET', 'POST'])
def ccd_exposure(camera_id):
    """Get or set exposure time for specified CCD camera"""
    if not device_proxy:
        return jsonify({"error": "Device not connected"}), 500
        
    if request.method == 'GET':
        try:
            attr_name = f"{camera_id}Exposure"
            exposure = device_proxy.read_attribute(attr_name).value
            return jsonify({
                "camera_id": camera_id,
                "exposure": exposure
            })
        except Exception as e:
            return jsonify({"error": str(e)}), 500
    else:  # POST
        try:
            data = request.get_json()
            exposure = data.get("exposure")
            if exposure is None:
                return jsonify({"error": "exposure parameter required"}), 400
                
            cmd_map = {
                "upperCCD1x": "setUpperCCD1xExposure",
                "upperCCD10x": "setUpperCCD10xExposure",
                "lowerCCD1x": "setLowerCCD1xExposure",
                "lowerCCD10x": "setLowerCCD10xExposure",
            }
            
            cmd_name = cmd_map.get(camera_id)
            if not cmd_name:
                return jsonify({"error": "Invalid camera_id"}), 400
                
            device_proxy.command_inout(cmd_name, exposure)
            return jsonify({
                "camera_id": camera_id,
                "exposure": exposure,
                "timestamp": time.time()
            })
        except Exception as e:
            return jsonify({"error": str(e)}), 500


@app.route('/api/ccd/<camera_id>/switch', methods=['POST'])
def ccd_switch(camera_id):
    """Turn CCD camera on/off"""
    if not device_proxy:
        return jsonify({"error": "Device not connected"}), 500
        
    try:
        data = request.get_json()
        on = data.get("on", True)
        
        cmd_map = {
            "upperCCD1x": "upperCCD1xSwitch",
            "upperCCD10x": "upperCCD10xSwitch",
            "lowerCCD1x": "lowerCCD1xSwitch",
            "lowerCCD10x": "lowerCCD10xSwitch",
        }
        
        cmd_name = cmd_map.get(camera_id)
        if not cmd_name:
            return jsonify({"error": "Invalid camera_id"}), 400
            
        device_proxy.command_inout(cmd_name, on)
        return jsonify({
            "camera_id": camera_id,
            "on": on,
            "timestamp": time.time()
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route('/api/stream/<camera_id>', methods=['GET'])
def stream_ccd_image(camera_id):
    """Stream images as MJPEG (Motion JPEG)"""
    def generate():
        while True:
            img_data = fetch_image_from_device(camera_id)
            if img_data:
                # Convert to JPEG if needed
                try:
                    img = Image.open(io.BytesIO(img_data))
                    if img.format != 'JPEG':
                        output = io.BytesIO()
                        img.convert('RGB').save(output, format='JPEG')
                        img_data = output.getvalue()
                except:
                    pass
                
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + img_data + b'\r\n')
            time.sleep(0.033)  # ~30 FPS
    
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')


if __name__ == '__main__':
    # Connect to device
    if not connect_device():
        print("Warning: Could not connect to Tango device. API will run in limited mode.")
    
    # Start background cache update thread
    cache_thread = threading.Thread(target=update_image_cache, daemon=True)
    cache_thread.start()
    
    # Create image cache directory
    os.makedirs(IMAGE_CACHE_DIR, exist_ok=True)
    
    print(f"Starting Image Stream API server on port {API_PORT}")
    print(f"API endpoints available at http://localhost:{API_PORT}/api/")
    print(f"Device: {DEVICE_NAME}")
    
    app.run(host='0.0.0.0', port=API_PORT, debug=False, threaded=True)

