import random
import string

def generate_random_name(length: int = 24) -> str:
    characters = string.ascii_letters + string.digits  # a-z, A-Z, 0-9
    return ''.join(random.choices(characters, k=length))