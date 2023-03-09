from bluepy import btle
import paho.mqtt.client as paho
import time

print("Connecting...")
dev = btle.Peripheral("57:78:C0:EE:38:3A")
 
print(dev)

def on_publish(client, userdata, mid):
    print("published")
    
client = paho.Client("smart_jacket_gw")
client.connect('broker.hivemq.com', 1883)
client.on_publish = on_publish
client.loop_start()
client.subscribe("status")


while True:
    my_device = btle.UUID("19b10000-0000-537e-4f6c-d104768a1214")
    my_device_service = dev.getServiceByUUID(my_device)
    uuidValue = btle.UUID("19b10000-2001-537e-4f6c-d104768a1214")
    inference_result = my_device_service.getCharacteristics(uuidValue)[0]
    payload = inference_result.read().decode('utf-8')
    # print(payload)
    client.publish('status', payload)
    time.sleep(1)
