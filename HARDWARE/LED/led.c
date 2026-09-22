#include "led.h"
#include "timer.h"

#define LED_PWM_PERIOD_US       2000U
#define LED_PWM_BREATH_DIVIDER  4U
#define LED_PWM_BREATH_STEP     4U
#define LED_PWM_GREEN_MASK      PIN_MASK(HW_LED_ACTIVE)
#define LED_PWM_BLUE_MASK       PIN_MASK(HW_LED_RESET)
#define LED_PWM_RED_MASK        PIN_MASK(HW_LED_HALT)
#define LED_PWM_ALL_MASK        (LED_PWM_GREEN_MASK | LED_PWM_BLUE_MASK | LED_PWM_RED_MASK)
#define LED_PWM_GPIO            ARM_PORT_GPIO(GET_PORT_FROM(HW_LED_ACTIVE))

typedef struct
{
    u16 timeUs;
    u16 offMask;
} ledPwmEdge_t;

static ledPwmMode_t g_ledPwmMode;
static u8 g_ledPwmRed;
static u8 g_ledPwmGreen;
static u8 g_ledPwmBlue;
static u8 g_ledPwmBreathColor;
static u8 g_ledPwmBreathLevel;
static u8 g_ledPwmBreathUp;
static u8 g_ledPwmBreathDivider;
static u8 g_ledPwmEdgeCount;
static u8 g_ledPwmEdgeIndex;
static ledPwmEdge_t g_ledPwmEdges[3];

#define LED_HANDLER_IDLE     0U
#define LED_HANDLER_RUNNING  1U
#define LED_HANDLER_PASS     2U
#define LED_HANDLER_FAIL     3U

static u8 g_ledUsbPending;
static u8 g_ledHandlerState;
static u8 g_ledFailOn;

static void ledPwmAddEdge(u16 timeUs, u16 offMask)
{
    u8 i;
    u8 insertAt;

    for (i = 0U; i < g_ledPwmEdgeCount; i++)
    {
        if (g_ledPwmEdges[i].timeUs == timeUs)
        {
            g_ledPwmEdges[i].offMask |= offMask;
            return;
        }
    }

    insertAt = g_ledPwmEdgeCount;
    while ((insertAt > 0U) && (g_ledPwmEdges[insertAt - 1U].timeUs > timeUs))
    {
        g_ledPwmEdges[insertAt] = g_ledPwmEdges[insertAt - 1U];
        insertAt--;
    }
    g_ledPwmEdges[insertAt].timeUs = timeUs;
    g_ledPwmEdges[insertAt].offMask = offMask;
    g_ledPwmEdgeCount++;
}

static void ledPwmAddChannel(u8 level, u16 mask, u16 *onMask)
{
    u16 timeUs;

    if (level == 0U)
        return;

    *onMask |= mask;
    if (level >= 255U)
        return;

    timeUs = (u16)(((u32)level * LED_PWM_PERIOD_US + 254U) / 255U);
    if (timeUs == 0U)
        timeUs = 1U;
    ledPwmAddEdge(timeUs, mask);
}

static void ledPwmUpdateBreathing(void)
{
    if (g_ledPwmMode != LED_PWM_MODE_FREE_CYCLE)
        return;

    g_ledPwmBreathDivider++;
    if (g_ledPwmBreathDivider < LED_PWM_BREATH_DIVIDER)
        return;
    g_ledPwmBreathDivider = 0U;

    if (g_ledPwmBreathUp != 0U)
    {
        if (g_ledPwmBreathLevel <= (u8)(255U - LED_PWM_BREATH_STEP))
            g_ledPwmBreathLevel = (u8)(g_ledPwmBreathLevel + LED_PWM_BREATH_STEP);
        else
        {
            g_ledPwmBreathLevel = 255U;
            g_ledPwmBreathUp = 0U;
        }
    }
    else if (g_ledPwmBreathLevel > LED_PWM_BREATH_STEP)
    {
        g_ledPwmBreathLevel = (u8)(g_ledPwmBreathLevel - LED_PWM_BREATH_STEP);
    }
    else
    {
        g_ledPwmBreathLevel = 0U;
        g_ledPwmBreathColor = (u8)((g_ledPwmBreathColor + 1U) % 3U);
        g_ledPwmBreathUp = 1U;
    }

    g_ledPwmRed = 0U;
    g_ledPwmGreen = 0U;
    g_ledPwmBlue = 0U;
    if (g_ledPwmBreathColor == 0U)
        g_ledPwmRed = g_ledPwmBreathLevel;
    else if (g_ledPwmBreathColor == 1U)
        g_ledPwmGreen = g_ledPwmBreathLevel;
    else
        g_ledPwmBlue = g_ledPwmBreathLevel;
}

static void ledPwmStartPeriod(void)
{
    u16 onMask = 0U;

    ledPwmUpdateBreathing();
    g_ledPwmEdgeCount = 0U;
    g_ledPwmEdgeIndex = 0U;
    ledPwmAddChannel(g_ledPwmGreen, LED_PWM_GREEN_MASK, &onMask);
    ledPwmAddChannel(g_ledPwmBlue, LED_PWM_BLUE_MASK, &onMask);
    ledPwmAddChannel(g_ledPwmRed, LED_PWM_RED_MASK, &onMask);

    LED_PWM_GPIO->BSRR = onMask;
    LED_PWM_GPIO->BRR = (u16)(LED_PWM_ALL_MASK & (u16)~onMask);
    if (g_ledPwmEdgeCount == 0U)
        timerLedPwmSchedule(LED_PWM_PERIOD_US);
    else
        timerLedPwmSchedule(g_ledPwmEdges[0].timeUs);
}//LED驱动代码	   

//初始化PB5和PE5为输出口.并使能这两个口的时钟		    
//LED IO初始化
void LED_Init(void)
{
	//RCC->APB2ENR|=1<<3;    //使能PORTB时钟	   	 
	//RCC->APB2ENR|=1<<6;    //使能PORTE时钟	
	//   	 
	//GPIOB->CRL&=0XFF0FFFFF; 
	//GPIOB->CRL|=0X00300000;//PB.5 推挽输出   	 
    //GPIOB->ODR|=1<<5;      //PB.5 输出高
	//										  
	//GPIOE->CRL&=0XFF0FFFFF;
	//GPIOE->CRL|=0X00300000;//PE.5推挽输出
	//GPIOE->ODR|=1<<5;      //PE.5输出高 

	// 打开端口时钟
	PORT_RCC_CLK(HW_LED_ACTIVE);
	PORT_RCC_CLK(HW_LED_RESET);
	PORT_RCC_CLK(HW_LED_HALT);
	// 设置端口方向
	PORT_SET_DIR_PP(HW_LED_ACTIVE);
	PORT_SET_DIR_PP(HW_LED_RESET);
	PORT_SET_DIR_PP(HW_LED_HALT);
	// 设置初始状态
	LED_ACTIVE_LIGHT;
	LED_RESET_LIGHT;
	LED_HALT_LIGHT;
}

static void ledPwmShowStatic(u16 onMask)
{
    timerLedPwmStop();
    LED_PWM_GPIO->BSRR = onMask;
    LED_PWM_GPIO->BRR = (u16)(LED_PWM_ALL_MASK & (u16)~onMask);
}

static void ledPwmApplyState(void)
{
    if (g_ledUsbPending != 0U)
    {
        ledPwmShowStatic(0U);
        return;
    }

    if (g_ledHandlerState == LED_HANDLER_RUNNING)
    {
        ledPwmShowStatic(LED_PWM_BLUE_MASK);
        return;
    }
    if (g_ledHandlerState == LED_HANDLER_PASS)
    {
        ledPwmShowStatic(LED_PWM_GREEN_MASK);
        return;
    }
    if (g_ledHandlerState == LED_HANDLER_FAIL)
    {
        g_ledFailOn = 1U;
        LED_PWM_GPIO->BSRR = LED_PWM_RED_MASK;
        LED_PWM_GPIO->BRR = (u16)(LED_PWM_ALL_MASK & (u16)~LED_PWM_RED_MASK);
        timerLedPwmStart(250000U);
        return;
    }

    if (g_ledPwmMode == LED_PWM_MODE_DISABLED)
    {
        ledPwmShowStatic(0U);
        return;
    }

    timerLedPwmStart(LED_PWM_PERIOD_US);
    ledPwmStartPeriod();
}

void LED_PWM_SetMode(ledPwmMode_t mode, u8 red, u8 green, u8 blue)
{
    timerLedPwmStop();
    g_ledPwmMode = mode;
    g_ledPwmEdgeCount = 0U;
    g_ledPwmEdgeIndex = 0U;

    if (mode == LED_PWM_MODE_FIXED)
    {
        g_ledPwmRed = red;
        g_ledPwmGreen = green;
        g_ledPwmBlue = blue;
    }
    else if (mode == LED_PWM_MODE_FREE_CYCLE)
    {
        g_ledPwmRed = 0U;
        g_ledPwmGreen = 0U;
        g_ledPwmBlue = 0U;
        g_ledPwmBreathColor = 0U;
        g_ledPwmBreathLevel = 0U;
        g_ledPwmBreathUp = 1U;
        g_ledPwmBreathDivider = 0U;
    }

    ledPwmApplyState();
}

void LED_PWM_USB_CommandBegin(void)
{
    if (g_ledUsbPending != 0xFFU)
        g_ledUsbPending++;
    g_ledHandlerState = LED_HANDLER_IDLE;
    ledPwmShowStatic(0U);
}

void LED_PWM_USB_ResponseComplete(void)
{
    if (g_ledUsbPending != 0U)
        g_ledUsbPending--;
    if (g_ledUsbPending == 0U)
        ledPwmApplyState();
}

void LED_PWM_HandlerBegin(void)
{
    g_ledUsbPending = 0U;
    g_ledHandlerState = LED_HANDLER_RUNNING;
    ledPwmApplyState();
}

void LED_PWM_HandlerResult(u8 success)
{
    g_ledHandlerState = (success != 0U) ? LED_HANDLER_PASS : LED_HANDLER_FAIL;
    ledPwmApplyState();
}

void LED_PWM_TimerIRQ(void)
{
    u16 elapsedUs;

    if (g_ledUsbPending != 0U)
        return;

    if (g_ledHandlerState == LED_HANDLER_FAIL)
    {
        g_ledFailOn ^= 1U;
        if (g_ledFailOn != 0U)
        {
            LED_PWM_GPIO->BSRR = LED_PWM_RED_MASK;
            LED_PWM_GPIO->BRR = (u16)(LED_PWM_ALL_MASK & (u16)~LED_PWM_RED_MASK);
        }
        else
        {
            LED_PWM_GPIO->BRR = LED_PWM_ALL_MASK;
        }
        timerLedPwmSchedule(250000U);
        return;
    }

    if (g_ledHandlerState != LED_HANDLER_IDLE ||
        g_ledPwmMode == LED_PWM_MODE_DISABLED)
        return;

    if (g_ledPwmEdgeIndex < g_ledPwmEdgeCount)
    {
        LED_PWM_GPIO->BRR = g_ledPwmEdges[g_ledPwmEdgeIndex].offMask;
        elapsedUs = g_ledPwmEdges[g_ledPwmEdgeIndex].timeUs;
        g_ledPwmEdgeIndex++;
        if (g_ledPwmEdgeIndex < g_ledPwmEdgeCount)
            timerLedPwmSchedule((u32)(g_ledPwmEdges[g_ledPwmEdgeIndex].timeUs - elapsedUs));
        else
            timerLedPwmSchedule((u32)(LED_PWM_PERIOD_US - elapsedUs));
        return;
    }

    ledPwmStartPeriod();
}
/// @brief 设置LED当前状态
/// @param index ：LED序号
/// @param state ：LED状态=0：关闭；=1：打开；=2：变化
void ledSetState(u8 index,u8 state)
{
	if(state  == 0){
		if(index == 0){	// Green
			LED_ACTIVE_DARK;
		}else if(index == 1){	// Blue
			LED_RESET_DARK;
		}else{			// Red
			LED_HALT_DARK;
		}
	}else if(state == 1){
		if(index == 0){
			LED_ACTIVE_LIGHT;
		}else if(index == 1){
			LED_RESET_LIGHT;
		}else{
			LED_HALT_LIGHT;
		}
	}else{
		if(index == 0){
			LED_ACTIVE_CHG;
		}else if(index == 1){
			LED_RESET_CHG;
		}else{
			LED_HALT_CHG;
		}
	}
}




