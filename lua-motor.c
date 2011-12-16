#define LUA_LIB

#include <time.h>
#include <stdint.h>
#include <lua.h>
#include <lauxlib.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#define LUA_MOTORLIBNAME "motor"

#define FPGA_SNOOP_CTL_ADR 0x00
#define FPGA_COMP_CTL_ADR 0x03
#define FPGA_EXT1_CTL_ADR 0x0c
#define FPGA_PWM_L_DIV_ADR 0x0d
#define FPGA_PWM_M_DIV_ADR 0x0e
#define FPGA_GENSTAT_ADR 0x10
#define FPGA_SERV_PER1_ADR 0x12 // LSB
#define FPGA_SERV_PER2_ADR 0x13
#define FPGA_SERV_PER3_ADR 0x14 // MSB
#define FPGA_SERV1_W1_ADR 0x15 // LSB
#define FPGA_SERV1_W2_ADR 0x16
#define FPGA_SERV1_W3_ADR 0x17 // MSB
#define FPGA_SERV2_W1_ADR 0x09 // LSB
#define FPGA_SERV2_W2_ADR 0x0a
#define FPGA_SERV2_W3_ADR 0x0b // MSB
#define FPGA_PLLSTAT_ADR 0x18
#define FPGA_BRD_CTL_ADR 0x19
#define FPGA_DIG_OUT_ADR 0x1a
#define FPGA_MOTEN_ADR 0x1b
#define FPGA_PWM1_ADR 0x1c
#define FPGA_PWM2_ADR 0x1d
#define FPGA_PWM3_ADR 0x1e
#define FPGA_PWM4_ADR 0x1f
#define FPGA_DIG_IN_ADR 0x20
#define FPGA_ADC_LSB_ADR 0x21
#define FPGA_ADC_MSB_ADR 0x22
#define FPGA_MOT_STAT_ADR 0x23
#define FPGA_SCAN_TST0_ADR 0x24
#define FPGA_SCAN_TST1_ADR 0x25
#define FPGA_SCAN_TST2_ADR 0x26
#define FPGA_SCAN_TST3_ADR 0x27

static int fd;
static char *i2c_device = "/dev/i2c-0";
static int dev_addr = 0x3C>>1;

int write_eeprom(int addr, int start_reg, uint8_t *buffer, int bytes) {
	uint8_t data[bytes+1];
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[1];

	if(!fd) {
		if ((fd = open(i2c_device, O_RDWR))==-1) {
			perror("Unable to open i2c device");
			return 1;
		}
	}

	// Set the address we'll read to the start address.
	data[0] = start_reg;
	memcpy(data+1, buffer, bytes);

	messages[0].addr = addr;
	messages[0].flags = 0;
	messages[0].len = bytes+1;
	messages[0].buf = data;

	packets.msgs = messages;
	packets.nmsgs = 1;

	if(ioctl(fd, I2C_RDWR, &packets) < 0) {
		perror("Unable to communicate with i2c device");
		return 1;
	}

	return 0;
}

int read_eeprom(int addr, int start_reg, uint8_t *buffer, int bytes) {
	int byte;
	//int page = 0;
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[2];
	uint8_t output;

	// On this chip, the upper two bits of the memory address are
	// represented in the i2c address, and the lower eight are clocked in
	// as the memory address. This gives a crude mechanism for 4 pages of
	// 256 bytes each.
	byte = (start_reg ) & 0xff;
	//page = (start_reg>>8) & 0x03;

	if (!fd) {
		if ((fd = open(i2c_device, O_RDWR))==-1) {
			perror("Unable to open i2c device");
			return 1;
		}
	}

	// Set the address we'll read to the start address.
	output = byte;

	messages[0].addr = addr;
	messages[0].flags = 0;
	messages[0].len = sizeof(output);
	messages[0].buf = &output;

	messages[1].addr = addr;
	messages[1].flags = I2C_M_RD;
	messages[1].len = bytes;
	messages[1].buf = (uint8_t *)buffer;

	packets.msgs = messages;
	packets.nmsgs = 2;

	if(ioctl(fd, I2C_RDWR, &packets) < 0) {
		perror("Unable to communicate with i2c device");
		return 1;
	}

	return 0;
}

static int
motor_get_adc(lua_State *L)
{
	uint8_t num;
	uint8_t buffer;
	int i;
	uint32_t val;

	num = luaL_checklong(L, 1);
	if (num < 1 || num > 8)
		return luaL_error(L, "ADC channel is out of range (1-8)");
	num--;

	// first conversion sets address
	read_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, 1);
	buffer &= 0xE0;
	buffer |= (num & 0x7);
	buffer |= 0x08;
	// setup addres
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer |= 0x10;
	// initiate conversion
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer &= 0xEF; // clear transfer
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));

	for (i=0; ((buffer & 0x08) == 0) && (i < 1000); i++)
		read_eeprom(dev_addr, FPGA_MOT_STAT_ADR, &buffer, 1);
	if ( i >= 1000 )
		printf( "Note: ADC timed out during readback\n" );

	// second conversion reads out the data
	read_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, 1);
	buffer &= 0xE0;
	buffer |= (num & 0x7);
	buffer |= 0x08;
	// setup addres
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer |= 0x10;
	// initiate conversion
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer &= 0xEF; // clear transfer
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));

	for (i=0; ((buffer & 0x08) == 0) && (i < 1000); i++)
		read_eeprom(dev_addr, FPGA_MOT_STAT_ADR, &buffer, 1);
	if( i >= 1000 )
		printf("Note: ADC timed out during readback\n");


	// read the computed value
	read_eeprom(dev_addr, FPGA_ADC_LSB_ADR, &buffer, 1);
	val = (buffer >> 4) & 0xF;
	read_eeprom(dev_addr, FPGA_ADC_MSB_ADR, &buffer, 1);
	val |= ((buffer << 4) & 0xF0);

	lua_pushnumber(L, val&0xFF);
	return 1;
}

static int
motor_sync_digital(lua_State *L)
{
	uint8_t buffer;
	read_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer &= 0xC7;
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer |= 0x20; // trigger sample
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer &= 0xDF; // kill sample
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer |= 0x10; // initiate transfer
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	buffer &= 0xEF; // clear transfer
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));
	return 0;
}

static int
motor_get_digital(lua_State *L)
{
	uint8_t buffer;
	read_eeprom(dev_addr, FPGA_DIG_OUT_ADR, &buffer, sizeof(buffer));
	lua_pushnumber(L, buffer);
	return 1;
}

static int
motor_set_digital_defer(lua_State *L)
{
	int new_value;
	uint8_t buffer;

	new_value = luaL_checklong(L, 1);

	if( (new_value > 255) || (new_value < 0) )
		return luaL_error(L, "Digital output is out of range (0-255)");

	buffer = new_value;
	write_eeprom(dev_addr, FPGA_DIG_OUT_ADR, &buffer, sizeof(buffer));

	return 0;
}


static int
motor_set_digital(lua_State *L)
{
	motor_set_digital_defer(L);
	return motor_sync_digital(L);
}



static int
motor_set_speed(lua_State *L)
{
	int speed_tmp, dir;
	uint8_t num;
	uint8_t prev, speed;

	num = luaL_checklong(L, 1);
	speed_tmp = luaL_checklong(L, 2);

	/* Read parameters and determine speed */
	if (num < 1 || num > 4)
		return luaL_error(L, "Motor number must be 1, 2, 3, or 4");

	if (speed_tmp < -255 || speed_tmp > 255)
		return luaL_error(L, "Speed is out of range (-255 to +255)");

	if (speed_tmp > 0) {
		dir = 1;
		speed = speed_tmp;
	}
	else if (speed_tmp == 0) {
		dir = 0;
		speed = 0;
	}
	else {
		dir = -1;
		speed = -speed_tmp;
	}
		

	/* Write out the "direction" value */
	read_eeprom(dev_addr, FPGA_MOTEN_ADR, &prev, sizeof(prev));

	if (dir == 1) {
		prev &= ~(0x3 << ((num - 1) * 2));
		prev |= 0x2 << ((num - 1) * 2);
	}
	else if (dir == -1) {
		prev &= ~(0x3 << ((num - 1) * 2));
		prev |= 0x1 << ((num - 1) * 2);
	}
	else if (dir == 0) {
		prev &= ~(0x3 << ((num - 1) * 2));
	}
	write_eeprom(dev_addr, FPGA_MOTEN_ADR, &prev, sizeof(prev));

	/* Bubble it through the scan chain */
	read_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &prev, sizeof(prev));
	prev &= 0xC7;
	prev |= 0x10; // initiate transfer
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &prev, sizeof(prev));
	prev &= 0xEF; // clear transfer
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &prev, sizeof(prev));


	/* Write out the PWM value */
	write_eeprom(dev_addr, FPGA_PWM1_ADR + num - 1, &speed, sizeof(speed));

	return 0;
}



static int
motor_set_angle(lua_State *L)
{
	uint8_t num, buffer, addr;
	uint32_t val;
	double angle;

	num = luaL_checklong(L, 1);
	angle = luaL_checknumber(L, 2);

	if ( !(num == 1 || num == 2) )
		return luaL_error(L,
			"Only channels 1 and 2 are supported for servo mode");

	if( (angle < 0.0) || (angle > 180.0) )
		return luaL_error(L,
			"Only angles from 0 to 180 degrees is allowed");

	// 1ms min, 1.5ms mid, 2ms max
	// convert to nanoseconds pulse length
	angle = (1000.0 * 1000.0) * (angle / 180.0);

	angle += 1000000.0; // add 1 ms for the minimum time
	angle = angle / 38.46153846; // divide to get # of clock cycles to wait

	// now bounds check a2 to prevent servo blow-outs
	val = (unsigned long) angle;
	if (val < 26000)
		val = 26000;
	if (val > 52000)
		val = 52000;
      
	// pick the correct chanel
	if (num == 1)
		addr = FPGA_SERV1_W1_ADR;
	else
		addr = FPGA_SERV2_W1_ADR;

	buffer = val & 0xFF;
	write_eeprom(dev_addr, addr, &buffer, sizeof(buffer));
	buffer = (val >> 8) & 0xFF;
	write_eeprom(dev_addr, addr + 1, &buffer, sizeof(buffer));
	buffer = (val >> 16) & 0xFF;
	write_eeprom(dev_addr, addr + 2, &buffer, sizeof(buffer));

	return 0;
}


static int
motor_set_type(lua_State *L)
{
	uint8_t num, type;
	uint8_t buffer;
	const char *type_str;

	num = luaL_checklong(L, 1);
	type_str = luaL_checklstring(L, 2, NULL);


	if (num != 1 && num != 2)
		return luaL_error(L, "Only channels 1 and 2 can switch types");

	if (*type_str != 's' && *type_str != 'm')
		return luaL_error(L,
		     "Only valid modes are s (servo mode) and m (motor mode)");

	buffer = 0x40;
	write_eeprom(dev_addr, FPGA_SERV_PER1_ADR, &buffer, sizeof(buffer));
	buffer = 0xEF;
	write_eeprom(dev_addr, FPGA_SERV_PER2_ADR, &buffer, sizeof(buffer));
	buffer = 0x07;
	write_eeprom(dev_addr, FPGA_SERV_PER3_ADR, &buffer, sizeof(buffer));

	// read back board control register value so as not to bash anything
	read_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, 1);
	if (*type_str == 's')
		buffer |= ((num == 1) ? 0x40 : 0x80);
	else if (*type_str == 'm')
		buffer &= ((num == 1) ? 0xBF : 0x7F);
	else
		return luaL_error(L, "Unrecognized type: %c", *type_str);
	write_eeprom(dev_addr, FPGA_BRD_CTL_ADR, &buffer, sizeof(buffer));

	return 0;
}

static int
netv_sleep(lua_State *L)
{
	struct timespec tv;
	lua_Number t;

	t = luaL_checknumber(L, 1);

	tv.tv_sec  = (int)t;
	tv.tv_nsec = (t-tv.tv_sec)*1000000000;
	nanosleep(&tv, NULL);
	return 0;
}


static const luaL_reg motorlib[] = {
	/* Digital */
	{"set_digital",		motor_set_digital},
	{"get_digital",		motor_get_digital},
	{"set_digital_defer",	motor_set_digital_defer},
	{"sync_digital",	motor_sync_digital},

	/* Analog */
	{"get_adc",		motor_get_adc},

	/* Motor type */
	{"set_type",		motor_set_type},

	/* Motor */
	{"set_speed",		motor_set_speed},

	/* Servos */
	{"set_angle",		motor_set_angle},

	{NULL, NULL}
};

static const luaL_reg netvlib[] = {
	{"sleep",		netv_sleep},
	{NULL, NULL}
};


/*
** Called when this module is loaded via 'require("motor")'
*/
LUALIB_API int luaopen_motor (lua_State *L) {
	luaL_register(L, LUA_MOTORLIBNAME, motorlib);
	luaL_register(L, "netv", netvlib);
	return 1;
}

