import re
import os

pattern = re.compile(r"(\w+)[\t ]+(\d+)[\t ]+(.+)")

s = ''
with os.popen("errno --list") as f:
    for l in f.readlines():
        s += l

for errno_name, errno_id, errno_description in pattern.findall(s):
    print(f'{{ {errno_id} ,{{ "{errno_name}", "{ errno_description }" }} }},')


