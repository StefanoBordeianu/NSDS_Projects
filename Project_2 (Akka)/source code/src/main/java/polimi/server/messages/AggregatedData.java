package polimi.server.messages;

import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import com.fasterxml.jackson.databind.ser.std.ToStringSerializer;
import com.fasterxml.jackson.datatype.jsr310.deser.LocalDateTimeDeserializer;
import lombok.Getter;
import lombok.Setter;

import java.time.LocalDateTime;

@Setter @Getter
public class AggregatedData {

    private final String key;
    private final double value;
    private final long sequenceNumber;

    @JsonSerialize(using = ToStringSerializer.class)
    @JsonDeserialize(using = LocalDateTimeDeserializer.class)
    private final LocalDateTime timestamp;
    private final String aggregationType;

    public AggregatedData(String key, double value, long sequenceNumber, LocalDateTime timestamp, String aggregationType) {
        this.key = key;
        this.value = value;
        this.sequenceNumber = sequenceNumber;
        this.timestamp = timestamp;
        this.aggregationType = aggregationType;
    }
}
