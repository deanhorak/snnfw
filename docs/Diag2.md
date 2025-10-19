graph TB
    subgraph "INPUT LAYER - RetinaAdapter"
        Input["<b>MNIST Image</b><br/>28×28 pixels<br/>Grayscale"]
        
        Grid["<b>8×8 Grid</b><br/>64 regions<br/>Each region: 3×3 pixels"]
        
        Sobel["<b>Sobel Edge Detection</b><br/>Gradient magnitude<br/>edge_threshold: 0.165"]
        
        Input --> Grid
        Grid --> Sobel
    end
    
    subgraph "NEURAL LAYER - 512 Neurons"
        direction TB
        
        R00["<b>Region [0,0]</b><br/>8 neurons<br/>(8 orientations)"]
        R01["<b>Region [0,1]</b><br/>8 neurons"]
        R07["<b>Region [0,7]</b><br/>8 neurons"]
        R70["<b>Region [7,0]</b><br/>8 neurons"]
        R77["<b>Region [7,7]</b><br/>8 neurons"]
        
        Sobel --> R00
        Sobel --> R01
        Sobel -.-> R07
        Sobel -.-> R70
        Sobel --> R77
        
        subgraph "Region [0,0] Detail"
            N0["<b>Neuron 0°</b><br/>Horizontal edges"]
            N45["<b>Neuron 45°</b><br/>Diagonal edges"]
            N90["<b>Neuron 90°</b><br/>Vertical edges"]
            N135["<b>Neuron 135°</b><br/>Diagonal edges"]
            Netc["<b>...</b><br/>8 orientations total"]
        end
        
        R00 --> N0
        R00 --> N45
        R00 --> N90
        R00 --> N135
        R00 --> Netc
    end
    
    subgraph "NEURON INTERNALS"
        direction LR
        
        NeuronBox["<b>Neuron</b><br/>windowSize: 200ms<br/>threshold: 0.7<br/>maxPatterns: 100"]
        
        Spikes["<b>Spike Window</b><br/>Rolling buffer<br/>Recent spike times"]
        
        Patterns["<b>Reference Patterns</b><br/>Up to 100 patterns<br/>Learned during training"]
        
        Strategy["<b>HybridStrategy</b><br/>merge_threshold: 0.85<br/>similarity_threshold: 0.7<br/>Prune + Consolidate"]
        
        NeuronBox --> Spikes
        NeuronBox --> Patterns
        NeuronBox --> Strategy
        
        Strategy -->|"MERGE<br/>(sim ≥ 0.85)"| Patterns
        Strategy -->|"BLEND<br/>(0.7 ≤ sim < 0.85)"| Patterns
        Strategy -->|"PRUNE<br/>(sim < 0.7)"| Patterns
    end
    
    subgraph "CLASSIFICATION - k-NN"
        direction TB
        
        Activation["<b>Activation Vector</b><br/>512 values<br/>(similarity scores)"]
        
        Training["<b>Training Set</b><br/>50,000 patterns<br/>(5,000 per digit)"]
        
        KNN["<b>k-NN Classifier</b><br/>k = 5<br/>MajorityVoting<br/>Cosine similarity"]
        
        Output["<b>Predicted Digit</b><br/>0-9<br/>94.94% accuracy"]
        
        R00 --> Activation
        R01 --> Activation
        R07 -.-> Activation
        R70 -.-> Activation
        R77 --> Activation
        
        Activation --> KNN
        Training --> KNN
        KNN --> Output
    end
    
    subgraph "STORAGE - ID-Based References"
        direction LR
        
        IDs["<b>All objects use 64-bit IDs</b><br/><br/>Neuron stores:<br/>• axonId (uint64_t)<br/>• dendriteIds (vector)<br/><br/>Cluster stores:<br/>• neuronIds (vector)<br/><br/>Layer stores:<br/>• clusterIds (vector)<br/><br/>Memory efficient!"]
    end
    
    style Input fill:#e1f5ff,stroke:#0288d1,stroke-width:2px
    style Grid fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    style Sobel fill:#ffccbc,stroke:#d84315,stroke-width:2px
    
    style R00 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    style R01 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    style R07 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    style R70 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    style R77 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    
    style N0 fill:#ffcdd2,stroke:#c62828,stroke-width:2px
    style N45 fill:#ffcdd2,stroke:#c62828,stroke-width:2px
    style N90 fill:#ffcdd2,stroke:#c62828,stroke-width:2px
    style N135 fill:#ffcdd2,stroke:#c62828,stroke-width:2px
    style Netc fill:#ffcdd2,stroke:#c62828,stroke-width:2px
    
    style NeuronBox fill:#f8bbd0,stroke:#880e4f,stroke-width:3px
    style Spikes fill:#e1bee7,stroke:#4a148c,stroke-width:2px
    style Patterns fill:#d1c4e9,stroke:#311b92,stroke-width:2px
    style Strategy fill:#bbdefb,stroke:#0d47a1,stroke-width:2px
    
    style Activation fill:#fff59d,stroke:#f57f17,stroke-width:2px
    style Training fill:#c5e1a5,stroke:#558b2f,stroke-width:2px
    style KNN fill:#90caf9,stroke:#1565c0,stroke-width:3px
    style Output fill:#a5d6a7,stroke:#2e7d32,stroke-width:3px
    
    style IDs fill:#ffe0b2,stroke:#e65100,stroke-width:2px
