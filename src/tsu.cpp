// TEMPORAL STOCHATICITY UNIT (TSU)
//Abhinav Basu
//28/01/2026
//FreeRTOS Kernel allows multitasking and scheduling jitter


#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "TSU_RESEARCH";
//TSU, ouput structure, it outputs 3 vairables (winner, latency, delta)

typedef struct {
    int winner;
    int64_t latency;
    int64_t delta;
} TSU_Result;

// --- NOISE GENERATOR TASK ---
// This creates background jitter by forcing the CPU to handle  non-deterministic interrupt/cache timings.
//Runs concurrently with the TSU race
//
void noise_generator_task(void* pvParameters) {
    volatile double dummy = 0;//prevents compiler optimizations and ensures the loop does actual work
    while(1) {                  //infinite loop
        for(int i=0; i<50; i++) {
            dummy += sin(i) * cos(i); // Heavy math to jitter the pipeline,
        }
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

// --- SYMMETRIC RACE ENGINE ---
// This function implements a single Temporal Stochastic Unit (TSU).
// A TSU makes a decision by allowing two competing temporal processes
// (A and B) to race toward biased time thresholds. The winner is the
// process that arrives first, and the decision confidence is encoded
// in the time it takes for the race to resolve.
TSU_Result run_symmetric_race(float bias) {

    // We inject a very small amount of *true hardware timing entropy*
    // by sampling the low bits of the CPU cycle counter.
    //
    // These low-order bits are highly sensitive to:
    //  - interrupt timing
    //  - cache misses
    //  - pipeline stalls
    //  - cross-core contention (from the noise task)
    //
    // This is NOT a pseudo-random number generator. It is physical,
    // microarchitectural timing noise.
    uint32_t cpu_jitter = (uint32_t)esp_cpu_get_cycle_count() % 5; 
    
    // Define the temporal thresholds ("drift targets") for the two competitors.
    //
    // Both start from a common baseline of 1000 microseconds.
    // The bias parameter shifts these thresholds asymmetrically:
    //  - Positive bias shortens A's path and lengthens B's path
    //  - Negative bias would do the opposite
    //
    // This is analogous to applying a drift term in a drift–diffusion model,
    // except the drift acts in *time* rather than state space.
    //
    // cpu_jitter slightly perturbs A's threshold to prevent deterministic
    // alignment and to ensure stochastic race resolution.
    uint32_t target_a = 1000 - (int)(bias * 500) + cpu_jitter; 
    uint32_t target_b = 1000 + (int)(bias * 500);

    // Record the start time of the decision process.
    // This marks t = 0 for the temporal race.
    int64_t start = esp_timer_get_time();

    // These variables will store the *actual arrival times*
    // of A and B once they cross their respective thresholds.
    // A value of 0 means "has not arrived yet".
    uint32_t arrival_a = 0, arrival_b = 0;

    // Main temporal race loop.
    //
    // The loop continues until BOTH competitors have crossed
    // their thresholds. This ensures symmetry: the decision
    // depends only on measured arrival times, not loop ordering.
    //
    // The passage of time here integrates:
    //  - hardware clock jitter
    //  - RTOS scheduling noise
    //  - interrupt latency
    //  - cache and pipeline effects
    while (arrival_a == 0 || arrival_b == 0) {

        // Current elapsed time since the start of the race.
        // This is the primary stochastic variable in the TSU.
        int64_t current = esp_timer_get_time() - start;

        // If competitor A has not yet arrived and the elapsed
        // time exceeds its biased temporal threshold, we record
        // the exact arrival time.
        //
        // This timestamp is not deterministic — it includes all
        // accumulated timing noise up to this moment.
        if (arrival_a == 0 && current >= target_a) {
            arrival_a = (uint32_t)current;
        }

        // Same logic for competitor B.
        // Importantly, A and B are treated symmetrically here.
        // No priority is given to either path.
        if (arrival_b == 0 && current >= target_b) {
            arrival_b = (uint32_t)current;
        }

        // Safety timeout to prevent pathological infinite loops.
        // This also models a real-world "decision timeout" where
        // unresolved races are forcibly terminated.
        if (current > 5000) break; 
    }

    // Decision extraction (measurement step).
    //
    // The winner is the competitor that arrived first in time.
    // This is the moment of "collapse" in the TSU:
    // probability is not computed — it is *resolved by timing*.
    int winner = (arrival_a <= arrival_b) ? 0 : 1;

    // Decision latency is the arrival time of the winner.
    //
    // Fast decisions correspond to strong bias / low uncertainty.
    // Slow decisions correspond to weak bias / high uncertainty.
    //
    // Thus, latency is a direct physical proxy for confidence.
    int64_t latency = (winner == 0) ? arrival_a : arrival_b;

    // Delta measures the separation between arrival times.
    //
    // A large delta means one competitor clearly dominated.
    // A small delta means the decision was marginal.
    //
    // This provides an additional confidence metric that
    // state-based probabilistic systems typically discard.
    int64_t delta = (arrival_a > arrival_b)
                  ? (arrival_a - arrival_b)
                  : (arrival_b - arrival_a);

    // Return the full TSU observable:
    //  - winner  : decision outcome
    //  - latency : confidence encoded in time
    //  - delta   : margin of victory
    return (TSU_Result){winner, latency, delta};
}


extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Launch the Noise Task on Core 1 (app_main usually runs on Core 0)
    xTaskCreatePinnedToCore(noise_generator_task, "jitter", 2048, NULL, 1, NULL, 1);

    printf("\nBias,Winner,Latency_us,Delta_us,Confidence\n");

    float test_biases[] = {0.0f, 0.25f, 0.5f, 0.75f, 0.95f};

    for (int b = 0; b < 5; b++) {
        float current_bias = test_biases[b];
        for (int i = 0; i < 100; i++) {
            TSU_Result res = run_symmetric_race(current_bias);
            float confidence = 1000.0f / (float)res.latency; 

            printf("%.2f,%d,%lld,%lld,%.4f\n", 
                   current_bias, res.winner, res.latency, res.delta, confidence);
            
            vTaskDelay(pdMS_TO_TICKS(5)); 
        }
    }
    printf("DONE\n");
}
