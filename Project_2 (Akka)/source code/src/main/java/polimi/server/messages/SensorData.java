package polimi.server.messages;


import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;
import polimi.server.utils.SequenceNumberGenerator;
import lombok.Getter;
import lombok.Setter;

@Getter  @Setter
public class SensorData {

    private final String key;
    private final Double value;
    private long sequenceNumber; // Unique identifier for message
    private int op;


    public SensorData( String key, Double value) {
        this(key, value, 0, SequenceNumberGenerator.generateSequenceNumber());
    }

    public SensorData( String key, Double value, int op) {
        this(key, value, op,SequenceNumberGenerator.generateSequenceNumber());
    }

    @JsonCreator
    public SensorData(@JsonProperty("key") String key,
                      @JsonProperty("value") Double value,
                      @JsonProperty("op") int op,
                      @JsonProperty("sequenceNumber") long sequenceNumber) {
        this.key = key;
        this.value = value;
        this.op = op;
        this.sequenceNumber = sequenceNumber;
    }

}
