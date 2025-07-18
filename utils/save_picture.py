import cv2
import numpy as np
import base64
from utils.generate_random_name import generate_random_name

class MyError(Exception):
    def __init__(self, message="Preprocessing Error"):
        self.message = message
        super().__init__(self.message)

def save_picture(base64_input_image):
    try:
        if not base64_input_image:
          raise MyError("Base64 image input is empty.")

        if "base64," in base64_input_image:
            base64_input_image = base64_input_image.split("base64,")[1]

        img_bytes = base64.b64decode(base64_input_image)

        np_arr = np.frombuffer(img_bytes, np.uint8)

        img = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

        if img is None:
          raise MyError("Hasil imdecode None (gambar tidak valid).")
        
    except Exception as e:
        raise MyError(f"Error decoding Base64 image: {e}")

    # Simpan image temporari untuk prediksi
    image_name = generate_random_name()
    temp_img_path = f"assets/images/{image_name}.jpg"
    success = cv2.imwrite(temp_img_path, img) 

    if not success:
      raise MyError("Failed to write temporary image file.")
    
    return temp_img_path