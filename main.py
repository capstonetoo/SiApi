import json
from fastapi import FastAPI, status, HTTPException
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from utils.send_email import sending_mail
from utils.save_picture import save_picture
from openai import OpenAI
from dotenv import load_dotenv
import os

load_dotenv()

client = OpenAI(
    api_key=os.getenv("OPENAI_API_KEY"),
)

app = FastAPI()

@app.get("/")
async def root():
    return "API is ready"

class schema_mail(BaseModel):
    temp: float 
    hum: float
    smoke: float
    flame: float
    img: str
    name: str
    email: str
    note: str 
    unique_code: str
    lat: str
    long: str

@app.post("/mail")
async def predict_fire_and_smoke(schema: schema_mail):
    try:
        temporary_img_path = save_picture(schema.img)

        def getHTML(body= ""):
            if body:
                return f"""
                <div style="padding: 16px;">
                    <h1 style="margin-bottom: 16px; color: tomato;">Laporan Dini Api</h1>
                    <h3 style="margin-bottom: 16px;">Kepada <span style="color: tomato;">{schema.name}</span></h3>
                    <h4 style="margin-bottom: 16px;">Pemilik Perangkat <span style="color: tomato;">{schema.unique_code}</span></h4>

                    <table style="width:100%; border-collapse: collapse;">
                      <tr>
                        <th style="padding: 8px; border: 1px solid #ddd; text-align: left; background-color:#f2f2f2;">Suhu</th>
                        <td style="padding: 8px; border: 1px solid #ddd;">{schema.temp}°</td>
                      </tr>
                      <tr>
                        <th style="padding: 8px; border: 1px solid #ddd; text-align: left; background-color:#f2f2f2;">Kelembapan</th>
                        <td style="padding: 8px; border: 1px solid #ddd;">{schema.hum}%</td>
                      </tr>
                      <tr>
                        <th style="padding: 8px; border: 1px solid #ddd; text-align: left; background-color:#f2f2f2;">Asap</th>
                        <td style="padding: 8px; border: 1px solid #ddd;">{schema.smoke > 400}</td>
                      </tr>
                      <tr>
                        <th style="padding: 8px; border: 1px solid #ddd; text-align: left; background-color:#f2f2f2;">Catatan</th>
                        <td style="padding: 8px; border: 1px solid #ddd;">{schema.note}</td>
                      </tr>
                      <tr>
                        <th style="padding: 8px; border: 1px solid #ddd; text-align: left; background-color:#f2f2f2;">Lokasi</th>
                        <td style="padding: 8px; border: 1px solid #ddd;">
                            <a href='https://www.google.com/maps?q={schema.lat},{schema.long}'>https://www.google.com/maps?q={schema.lat},{schema.long}</span></a>
                        </td>
                      </tr>
                    </table>

                    <p style="margin-bottom: 16px;">{body}</p>
                    <p >Hormat Kami Tim <span style="color: tomato;">SiApi</span></p>

                </div>
                """
            
            else:
                return f"""
                <div style="display: flex; flex-direction: column; gap: 16px; padding: 16px;">
                    <h2 style="margin-bottom: 16px; color: tomato;">Laporan Dini Api</h2>
                    <h3 style="margin-bottom: 16px;">Kepada <span style="color: tomato;">{{schema.name}}</span></h3>
                    <h4 style="margin-bottom: 16px;">Pemilik Perangkat <span style="color: tomato;">{{schema.unique_code}}</span></h4>

                    <p classname="mail-body" style="margin-bottom: 16px;">{{body}}</p>
                    <p style="margin-bottom: 16px;">Hormat Kami Tim <span style="color: tomato;">SiApi</span></p>
                </div>
                """


        if temporary_img_path:
            payload = {
                "prompt": """
                    Anda adalah agen (SiApi). 
                    Berdasarkan temperatur, kelembaban, deteksi asap, dan analisis gambar, tentukan apakah ada indikasi bahaya kebakaran. 
                    Jika ada, siapkan email laporan dan panggil fungsi `send_mail` dengan `sending: true` dan isi email yang sesuai. 
                    Jika tidak ada bahaya, panggil fungsi `send_mail` dengan `sending: false` dan isi email yang menjelaskan kondisi aman. 
                    Isi email hanya berisi plain text dan bertujuan agar pengguna dapat memahami informasi yang digambarkan oleh payload.
                    Deskripsikan juga gambar berdasarkan hasil analisis Anda. 
                    isi email tidak memerlukan kata penutup seperti 'hormat kami' dan sebagainya karena telah disiapkan oleh format.
                    Kembalikan hanya isi dari tool_calls nya tidak perlu content dari chatCompletionMessage.
                """,
                "format": getHTML(),
                "temp": schema.temp,
                "hum": schema.hum,
                "smoke": schema.smoke > 400,
                "flame": 0 < schema.flame < 500,
                "name": schema.name,
                "user_given_note": schema.note,
                "unique_code": schema.unique_code
            }

            tools = [
                {
                    "type": "function",
                    "function": {
                        "name": "send_mail",
                        "description": "Fungsi untuk mengirimkan email pada klien diputuskan berdasarkan urgensi situasi yang dianalisis",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "sending": {
                                    "type": "boolean",
                                    "description": "false jika hasil analisis menyatakan tidak adanya situasi bahaya yang memungkinkan terjadinya kebakaran, true jika hasil analisis menyatakan sebaliknya"
                                },
                                "body": {
                                    "type": "string",
                                    "description": "isi dari email yang dikirimkan, hasil analisis situasi"
                                }
                            },
                            "required": ["sending", "body"]
                        }
                    }

                }
            ]

            response = client.chat.completions.create(
                model="gpt-4o",
                messages=[
                    {
                        "role": "user",
                        "content": [
                            {"type": "text", "text": str(payload)},
                            {
                                "type": "image_url",
                                "image_url": {
                                    "url": f"data:image/jpeg;base64,{schema.img}"
                                },
                            },
                        ],
                    }
                ],
                tools= tools,
                tool_choice= "auto",
                max_tokens=300,
            )

            message = response.choices[0].message

            print(message)

            if message.tool_calls:
                for tool_call in message.tool_calls:

                    if tool_call.type == "function":

                        function_args_str = tool_call.function.arguments
                        function_args = json.loads(function_args_str)

                        sending = function_args.get("sending")
                        body = function_args.get("body")

                        if (sending):
                            await sending_mail(schema.email, getHTML(body), temporary_img_path)

                            return JSONResponse(
                                content={"message": "Email terkirim"},
                                status_code= status.HTTP_200_OK
                            )
                        
                        else:
                            return JSONResponse(
                                content={"message": "Tidak terdeteksi potensi kebakaran, tidak mengirim email"},
                                status_code= status.HTTP_200_OK
                            )
                        
            else:
                return JSONResponse(
                    content={"message": "Tidak terdeteksi potensi kebakaran, tidak mengirim email"},
                    status_code= status.HTTP_200_OK
                )
        
    except Exception as e:
        print(e)
        return HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Internal Server Error"
        )
    
    finally:
        if os.path.exists(temporary_img_path):
            os.remove(temporary_img_path)
