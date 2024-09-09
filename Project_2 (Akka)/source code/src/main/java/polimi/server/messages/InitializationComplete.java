package polimi.server.messages;

import lombok.Getter;

@Getter
public class InitializationComplete {

    private final String key;

    public InitializationComplete(String key) {
        this.key = key;
    }
}
