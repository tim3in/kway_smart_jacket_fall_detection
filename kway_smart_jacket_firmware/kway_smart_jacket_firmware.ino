#include <Arduino.h>
#include "Nicla_System.h"              // Nicla library to be able to enable battery charging and LED control
#include "Arduino_BHY2.h"              // Arduino library to read data from every built-in sensor of the Nicla Sense ME
#include <ArduinoBLE.h>                // Bluetooth® Low Energy library for the Nicla Sense Me
#include "K-Way_Smart_Jacket_HAR_inferencing.h" // Edge Impulse Arduino Library of your trained model
#define  DEBUG_FLAG 0
#include <Wire.h>

#define BLE_SENSE_UUID(val) ("19b10000-" val "-537e-4f6c-d104768a1214")
const int VERSION = 0x00000000;

//----------------------------------------------------------------------------------------
//BLEService inferenceService("00002a50-0000-1000-8000-00805f9b34fb");  
//BLEStringCharacteristic inferenceCharacteristic("00002a53-0000-1000-8000-00805f9b34fb", BLERead | BLENotify, 56);

BLEService inferenceService(BLE_SENSE_UUID("0000"));
BLEUnsignedIntCharacteristic versionCharacteristic(BLE_SENSE_UUID("1001"), BLERead);
BLEStringCharacteristic inferenceCharacteristic(BLE_SENSE_UUID("2001"), BLERead | BLENotify, 10);
//----------------------------------------------------------------------------------------


/** Struct to link sensor axis name to sensor value function */
typedef struct{
    const char *name;
    float (*get_value)(void);

}eiSensors;

/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2    9.80665f

/** Number sensor axes used */
#define NICLA_N_SENSORS     17


/* Private variables ------------------------------------------------------- */
static const bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal

SensorXYZ accel(SENSOR_ID_ACC);
SensorXYZ gyro(SENSOR_ID_GYRO);
SensorOrientation ori(SENSOR_ID_ORI);
SensorQuaternion rotation(SENSOR_ID_RV);
Sensor temp(SENSOR_ID_TEMP);
Sensor baro(SENSOR_ID_BARO);
Sensor hum(SENSOR_ID_HUM);
Sensor gas(SENSOR_ID_GAS);

static bool ei_connect_fusion_list(const char *input_list);
static float get_accX(void){return (accel.x() * 8.0 / 32768.0) * CONVERT_G_TO_MS2;}
static float get_accY(void){return (accel.y() * 8.0 / 32768.0) * CONVERT_G_TO_MS2;}
static float get_accZ(void){return (accel.z() * 8.0 / 32768.0) * CONVERT_G_TO_MS2;}
static float get_gyrX(void){return (gyro.x() * 8.0 / 32768.0) * CONVERT_G_TO_MS2;}
static float get_gyrY(void){return (gyro.y() * 8.0 / 32768.0) * CONVERT_G_TO_MS2;}
static float get_gyrZ(void){return (gyro.z() * 8.0 / 32768.0) * CONVERT_G_TO_MS2;}
static float get_oriHeading(void){return ori.heading();}
static float get_oriPitch(void){return ori.pitch();}
static float get_oriRoll(void){return ori.roll();}
static float get_rotX(void){return rotation.x();}
static float get_rotY(void){return rotation.y();}
static float get_rotZ(void){return rotation.z();}
static float get_rotW(void){return rotation.w();}
static float get_temperature(void){return temp.value();}
static float get_barrometric_pressure(void){return baro.value();}
static float get_humidity(void){return hum.value();}
static float get_gas(void){return gas.value();}

static int8_t fusion_sensors[NICLA_N_SENSORS];
static int fusion_ix = 0;

/** Used sensors value function connected to label name */
eiSensors nicla_sensors[] =
{
    "accX", &get_accX,
    "accY", &get_accY,
    "accZ", &get_accZ,
    "gyrX", &get_gyrX,
    "gyrY", &get_gyrY,
    "gyrZ", &get_gyrZ,
    "heading", &get_oriHeading,
    "pitch", &get_oriPitch,
    "roll", &get_oriRoll,
    "rotX", &get_rotX,
    "rotY", &get_rotY,
    "rotZ", &get_rotZ,
    "rotW", &get_rotW,
    "temperature", &get_temperature,
    "barometer", &get_barrometric_pressure,
    "humidity", &get_humidity,
    "gas", &get_gas,
};

void sendInferenceOverBLE(String inferenceResult);


/**
* @brief      Arduino setup function
*/
void setup()
{
    /* Init serial */
    Serial.begin(115200);
    // comment out the below line to cancel the wait for USB connection (needed for native USB)
    while (!Serial);
    Serial.println("Edge Impulse Sensor Fusion Inference\r\n");

    /* Connect used sensors */
    if(ei_connect_fusion_list(EI_CLASSIFIER_FUSION_AXES_STRING) == false) {
        ei_printf("ERR: Errors in sensor list detected\r\n");
        return;
    }

    /* Init & start sensors */
    BHY2.begin(NICLA_I2C);
   
    accel.begin();
    gyro.begin();
    ori.begin();
    rotation.begin();
    temp.begin();
    baro.begin();
    hum.begin();
    gas.begin();


    //---------------------------------------------------------------------------------------
       if (!BLE.begin()) {   // initialize BLE
      Serial.println("starting BLE failed!");
      while (1);
    }

    BLE.setLocalName("Tim-Nicla");  // Set device name 
    BLE.setDeviceName("Tim-Nicla"); 
    //Set the minimum and maximum desired connection intervals in units of 1.25 ms.
    //Bluetooth LE desired connection Interval 160ms - 200ms. Central has the final word! 
    //BLE.setConnectionInterval(160, 200);
    BLE.setAdvertisedService(inferenceService); // Advertise service
    inferenceService.addCharacteristic(inferenceCharacteristic); // Add characteristic to service
   
    inferenceCharacteristic.writeValue("inference"); // Set first value string
    //Set the advertising interval in units of 0.625 ms
    //Bluetooth LE advertising interval around 50ms
   // BLE.setAdvertisingInterval(80);

   versionCharacteristic.setValue(VERSION);
    BLE.addService(inferenceService); // Add service
    BLE.advertise();  // Start advertising
    #if (DEBUG_FLAG==1)
        Serial.print("Peripheral device MAC: ");
        Serial.println(BLE.address());
        Serial.println("Waiting for connections...");
    #endif


    //---------------------------------------------------------------------------------------
  
    
}

/**
* @brief      Get data and run inferencing
*/
void loop()
{

    //-------------------------------------------------------------------
      BLEDevice central = BLE.central();
      String inferenceResult = "";
  //----------------------------------------------------------------------

  
    ei_printf("\nStarting inferencing in 2 seconds...\r\n");

    delay(2000);

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != fusion_ix) {
        ei_printf("ERR: Nicla sensors don't match the sensors required in the model\r\n"
        "Following sensors are required: %s\r\n", EI_CLASSIFIER_FUSION_AXES_STRING);
        return;
    }

    ei_printf("Sampling...\r\n");

    // Allocate a buffer here for the values we'll read from the IMU
    float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
        // Determine the next tick (and then sleep later)
        int64_t next_tick = (int64_t)micros() + ((int64_t)EI_CLASSIFIER_INTERVAL_MS * 1000);


        // Update function should be continuously polled
        BHY2.update();

        for(int i = 0; i < fusion_ix; i++) {
            buffer[ix + i] = nicla_sensors[fusion_sensors[i]].get_value();
        }

        int64_t wait_time = next_tick - (int64_t)micros();

        if(wait_time > 0) {
            delayMicroseconds(wait_time);
        }
    }

    // Turn the raw buffer in a signal which we can the classify
    signal_t signal;
    int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0) {
        ei_printf("ERR:(%d)\r\n", err);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR:(%d)\r\n", err);
        return;
    }

    // print the predictions
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.):\r\n",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("%s: %.5f\r\n", result.classification[ix].label, result.classification[ix].value);
        if(result.classification[ix].value >0.7 && inferenceResult != result.classification[ix].label){
              inferenceResult = result.classification[ix].label;
              sendInferenceOverBLE(inferenceResult);
            } 
        
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: %.3f\r\n", result.anomaly);
#endif


}

#if !defined(EI_CLASSIFIER_SENSOR) || (EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_FUSION && EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_ACCELEROMETER)
#error "Invalid model for current sensor"
#endif

static int8_t ei_find_axis(char *axis_name)
{
    int ix;
    for(ix = 0; ix < NICLA_N_SENSORS; ix++) {
        if(strstr(axis_name, nicla_sensors[ix].name)) {
            return ix;
        }
    }
    return -1;
}


static bool ei_connect_fusion_list(const char *input_list)
{
    char *buff;
    bool is_fusion = false;

    /* Copy const string in heap mem */
    char *input_string = (char *)ei_malloc(strlen(input_list) + 1);
    if (input_string == NULL) {
        return false;
    }
    memset(input_string, 0, strlen(input_list) + 1);
    strncpy(input_string, input_list, strlen(input_list));

    /* Clear fusion sensor list */
    memset(fusion_sensors, 0, NICLA_N_SENSORS);
    fusion_ix = 0;

    buff = strtok(input_string, "+");

    while (buff != NULL) { /* Run through buffer */
        int8_t found_axis = 0;

        is_fusion = false;
        found_axis = ei_find_axis(buff);

        if(found_axis >= 0) {
            if(fusion_ix < NICLA_N_SENSORS) {
                fusion_sensors[fusion_ix++] = found_axis;
            }
            is_fusion = true;
        }

        buff = strtok(NULL, "+ ");
    }

    ei_free(input_string);

    return is_fusion;
}

void sendInferenceOverBLE(String inferenceResult) {
  
/*#if (DEBUG_FLAG==1)
     Serial.println("Sending inference result over BLE..."); // print it to serial consle 
#endif
*/   Serial.println("Sending inference result over BLE...");
     inferenceCharacteristic.writeValue(inferenceResult); // Write value
        
}
