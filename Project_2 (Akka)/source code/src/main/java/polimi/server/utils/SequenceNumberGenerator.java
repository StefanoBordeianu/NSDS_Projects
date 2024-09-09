package polimi.server.utils;


import polimi.server.messages.SensorData;

public class SequenceNumberGenerator {

    private static long counter = 0;

    public static long generateSequenceNumber() {
        synchronized (SensorData.class) { // Ensure thread-safety for counter access
            return counter++;
        }
    }
}
