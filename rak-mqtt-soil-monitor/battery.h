#include <soc/soc.h>
#include <stdint.h>
#include <Arduino.h>

#define PIN_VBAT WB_A0
#define VBAT_MV_PER_LSB (0.73242188F) // 3.0V ADC range and 12 - bit ADC resolution = 3000mV / 4096
#define VBAT_DIVIDER (0.6F)       // 1.5M + 1M voltage divider on VBAT = (1.5M / (1M + 1.5M))
#define VBAT_DIVIDER_COMP (1.45F)   // Compensation factor for the VBAT divider, depend on the board
#define REAL_VBAT_MV_PER_LSB (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB)
#define NO_OF_SAMPLES   10          //Multisampling

#define LUT_POINTS 20
#define LUT_VREF_LOW 1000
#define LUT_VREF_HIGH 1200
#define LUT_ADC_STEP_SIZE 64
#define LUT_POINTS 20
#define LUT_LOW_THRESH 2880
#define LUT_HIGH_THRESH (LUT_LOW_THRESH + LUT_ADC_STEP_SIZE)

#define VREF_OFFSET 1100
#define DR_REG_EFUSE_BASE 0x3ff5A000
#define EFUSE_BLK0_RDATA4_REG (DR_REG_EFUSE_BASE + 0x010)
#define VREF_REG EFUSE_BLK0_RDATA4_REG
#define VREF_MASK 0x1F
#define VREF_FORMAT 0
#define VREF_STEP_SIZE 7

#define EFUSE_ADC_VREF 0x0000001F
#define EFUSE_ADC_VREF_V 0x1F
#define EFUSE_ADC_VREF_S 8
#define EFUSE_ADC_VREF_M ((EFUSE_ADC_VREF_V) << (EFUSE_ADC_VREF_S))

uint32_t vbat_pin = PIN_VBAT;
char data[32] = {0};

//20 Point lookup tables, covering ADC readings from 2880 to 4096, step size of 64
static const uint32_t lut_adc1_low[LUT_POINTS] = { 2240, 2297, 2352, 2405, 2457, 2512, 2564, 2616, 2664, 2709,
                                                   2754, 2795, 2832, 2868, 2903, 2937, 2969, 3000, 3030, 3060 };
static const uint32_t lut_adc1_high[LUT_POINTS] = { 2667, 2706, 2745, 2780, 2813, 2844, 2873, 2901, 2928, 2956,
                                                    2982, 3006, 3032, 3059, 3084, 3110, 3135, 3160, 3184, 3209 };
//Only call when ADC reading is above threshold
static uint32_t calculate_voltage_lut(uint32_t adc, uint32_t vref) {
  //Get index of lower bound points of LUT
  uint32_t i = (adc - LUT_LOW_THRESH) / LUT_ADC_STEP_SIZE;

  //Let the X Axis be Vref, Y axis be ADC reading, and Z be voltage
  int x2dist = LUT_VREF_HIGH - vref;                                  //(x2 - x)
  int x1dist = vref - LUT_VREF_LOW;                                   //(x - x1)
  int y2dist = ((i + 1) * LUT_ADC_STEP_SIZE) + LUT_LOW_THRESH - adc;  //(y2 - y)
  int y1dist = adc - ((i * LUT_ADC_STEP_SIZE) + LUT_LOW_THRESH);      //(y - y1)

  //For points for bilinear interpolation
  int q11 = lut_adc1_low[i];       //Lower bound point of low_vref_curve
  int q12 = lut_adc1_low[i + 1];   //Upper bound point of low_vref_curve
  int q21 = lut_adc1_high[i];      //Lower bound point of high_vref_curve
  int q22 = lut_adc1_high[i + 1];  //Upper bound point of high_vref_curve

  //Bilinear interpolation
  //Where z = 1/((x2-x1)*(y2-y1)) * ( (q11*x2dist*y2dist) + (q21*x1dist*y2dist) + (q12*x2dist*y1dist) + (q22*x1dist*y1dist) )
  int voltage = (q11 * x2dist * y2dist) + (q21 * x1dist * y2dist) + (q12 * x2dist * y1dist) + (q22 * x1dist * y1dist);
  voltage += ((LUT_VREF_HIGH - LUT_VREF_LOW) * LUT_ADC_STEP_SIZE) / 2;  //Integer division rounding
  voltage /= ((LUT_VREF_HIGH - LUT_VREF_LOW) * LUT_ADC_STEP_SIZE);      //Divide by ((x2-x1)*(y2-y1))
  return (uint32_t)voltage;
}

static inline int decode_bits(uint32_t bits, uint32_t mask, bool is_twos_compl) {
  int ret;
  if (bits & (~(mask >> 1) & mask)) {
    //Check sign bit (MSB of mask)
    //Negative
    if (is_twos_compl) {
      ret = -(((~bits) + 1) & (mask >> 1));  //2's complement
    } else {
      ret = -(bits & (mask >> 1));  //Sign-magnitude
    }
  } else {
    //Positive
    ret = bits & (mask >> 1);
  }
  return ret;
}

static uint32_t read_efuse_vref(void) {
  //eFuse stores deviation from ideal reference voltage
  uint32_t ret = VREF_OFFSET;  //Ideal vref
  uint32_t bits = REG_GET_FIELD(VREF_REG, EFUSE_ADC_VREF);
  ret += decode_bits(bits, VREF_MASK, VREF_FORMAT) * VREF_STEP_SIZE;
  return ret;  //ADC Vref in mV
}

static inline uint32_t interpolate_two_points(uint32_t y1, uint32_t y2, uint32_t x_step, uint32_t x) {
  //Interpolate between two points (x1,y1) (x2,y2) between 'lower' and 'upper' separated by 'step'
  return ((y1 * x_step) + (y2 * x) - (y1 * x) + (x_step / 2)) / x_step;
}

uint32_t esp_adc_cal_raw_to_voltage(uint32_t adc_raw) {
  uint32_t vref = read_efuse_vref();

  uint32_t lut_voltage = calculate_voltage_lut(adc_raw, vref);
  if (adc_raw >= LUT_LOW_THRESH) {
    if (adc_raw <= LUT_HIGH_THRESH) {
      uint32_t linear_voltage = (((((vref * 196602 / 4096)) * adc_raw) + (65536 / 2)) / 65536) + 142;
      return interpolate_two_points(linear_voltage, lut_voltage, LUT_ADC_STEP_SIZE, (adc_raw - LUT_LOW_THRESH));
    } else {
      return lut_voltage;
    }
  } else if (adc_raw == 0) {
    return 0;
  } else {
    return ((((((vref * 196602 / 4096) * adc_raw) + (65536 / 2))) / 65536) + 142);
  }
}

void setup_battery(){
  Serial.print("Battery setup... ");
  adcAttachPin(vbat_pin);
  analogSetAttenuation(ADC_11db);
  // Set the resolution to 12-bit (0..4095)
  analogReadResolution(12); // Can be 8, 10, 12 or 14
  Serial.println("complete");
}

/* TODO:
  This percentage calculation needs adjustment.
  18650 LiIon cells range from 2.5-4.2V, can we find a good slope?
  https://www.reddit.com/r/18650masterrace/comments/ibhp9i/comment/g20wcv7/?utm_source=reddit&utm_medium=web2x&context=3
  https://www.calculator.net/slope-calculator.html?type=1&x11=0&y11=3&x12=100&y12=4.2&x=83&y=16
*/
uint8_t mvToPercent(float mvolts){
  if (mvolts < 3000)
    return 0;

  if (mvolts < 4200){
    mvolts -= 3300;
    return mvolts / 30;
  }

  mvolts -= 3600;
  return 10 + (mvolts * 0.15F); // that's mvolts /6.66666666
}

float read_battery_mv(void){
  int i;
  float raw;
  float mv;

  raw = 0;
  for (i = 0; i < NO_OF_SAMPLES; i++){
    raw += analogRead(vbat_pin);
  }
  raw /= NO_OF_SAMPLES;
  mv = esp_adc_cal_raw_to_voltage(raw * REAL_VBAT_MV_PER_LSB);

  return mv * (1/VBAT_DIVIDER);
}

uint8_t read_battery_percent(){
  // Get a raw ADC reading
  float vbat_mv = read_battery_mv();

  // Convert from raw mv to percentage (based on LIPO chemistry)
  uint8_t vbat_per = mvToPercent(vbat_mv);
  return vbat_per;
}
