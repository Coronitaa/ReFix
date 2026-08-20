# Re:Photon — Cloud Infrastructure & Scalability

## 1. Separation of Control Plane & Data Plane

To ensure maximum performance and minimum infrastructure costs for 100–500 concurrent players, Re:Photon splits its cloud infrastructure into two distinct planes:

```
                              [ Game Clients ]
                               /            \
                (HTTPS/WSS)   /              \   (UDP Relay)
                             v                v
                 +-------------------+   +--------------------+
                 |   Control Plane   |   |     Data Plane     |
                 | (Cloudflare Edge) |   | (Regional Relays)  |
                 +-------------------+   +--------------------+
                 | - Authentication  |   | - ENet UDP Relay   |
                 | - Region Registry |   | - Fast Room State  |
                 | - Matchmaking API |   | - Voice SFU Stream |
                 | - Server Health   |   | - Tick Broadcast   |
                 +-------------------+   +--------------------+
```

---

## 2. Regional GameServer Pools

1. **Regional Nodes:**
   * **`sa` (South America - São Paulo)**
   * **`us` (North America - East/West)**
   * **`eu` (Europe - Frankfurt)**
   * **`asia` (Asia - Singapore/Tokyo)**

2. **Least-Loaded Server Allocation:**
   The Control Plane queries the `RegionRegistry` and routes new room allocations to the Game Server with lowest CPU load and room count:
   $$\text{Score}(s) = \alpha \cdot \text{RoomCount}(s) + \beta \cdot \text{CPULoad}(s)$$

---

## 3. Cost Projections

* **Development Phase:** $0 (Localhost / LAN).
* **Beta Phase (<50 concurrent players):** ~$0 – $5 / month (Cloudflare Free Tier + 1 Low-cost VPS).
* **Production Scaling (100–500 players):** ~$15 – $30 / month across 2-3 regional VPS relays.
