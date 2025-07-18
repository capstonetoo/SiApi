from fastapi_mail import ConnectionConfig, FastMail, MessageSchema, MessageType 
from dotenv import load_dotenv
import os

load_dotenv()

conf = ConnectionConfig(
    MAIL_USERNAME = os.getenv("MAIL_USERNAME"),
    MAIL_PASSWORD = os.getenv("MAIL_PASSWORD"),
    MAIL_FROM = os.getenv("MAIL_FROM"),
    MAIL_PORT = os.getenv("MAIL_PORT"),
    MAIL_SERVER = os.getenv("MAIL_SERVER"),
    MAIL_FROM_NAME="Sistem Dini Pendeteksi Api (SiApi)",
    MAIL_STARTTLS = True,
    MAIL_SSL_TLS = False,
    USE_CREDENTIALS = True,
    VALIDATE_CERTS = True
)

async def sending_mail(email, body, image_path):

    message = MessageSchema(
        subject="Peringatan Dini Api",
        recipients=[email],
        body=body,
        subtype=MessageType.html,
        attachments=[image_path]
    )

    fm = FastMail(conf)

    try:
        message = MessageSchema(
            subject="Peringatan Dini Api",
            recipients=[email],
            body=body,
            subtype=MessageType.html,
            attachments=[image_path]  # path string yang valid
        )

        await fm.send_message(message)
        print("✅ Email sent")

    finally:
        if os.path.exists(image_path):
            os.remove(image_path)

    return {"message": "email has been sent"} 