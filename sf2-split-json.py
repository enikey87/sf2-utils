import os
import json
import re
import sys

def list_sf2_files(directory):
    pattern = re.compile(r'^(\d{3})-(\d{3})-([^\\/]+)\.sf2$')
    
    bank_preset_mapping = {}
    
    for filename in os.listdir(directory):
        match = pattern.match(filename)
        if match:
            bank, preset, name = match.groups()
            bank = int(bank)
            preset = int(preset)
            
            if bank not in bank_preset_mapping:
                bank_preset_mapping[bank] = {}
            bank_preset_mapping[bank][preset] = filename
    
    json_descriptor = json.dumps(bank_preset_mapping, indent=4)
    
    return json_descriptor

if __name__ == "__main__":
    # Get directory from command line or use current working directory
    directory_path = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    json_descriptor = list_sf2_files(directory_path)
    print(json_descriptor)

