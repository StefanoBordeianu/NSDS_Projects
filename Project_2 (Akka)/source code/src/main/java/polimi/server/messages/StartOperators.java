package polimi.server.messages;

import lombok.Getter;

@Getter
public class StartOperators {

    private final int numOperators = 4;
    private String key;

    public StartOperators(String key) {
        this.key = key;
    }
}
